// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mnemonicimportdialog.h>
#include <qt/forms/ui_mnemonicimportdialog.h>
#include <qt/walletmodel.h>
#include <qt/mnemonic.h>

#include <QMessageBox>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QThreadPool>
#include <qt/guiutil.h>
#include <util.h>
#include <support/cleanse.h>

#include <vector>

MnemonicImportDialog::MnemonicImportDialog(QWidget *parent, WalletModel *_model) :
    QDialog(parent),
    ui(new Ui::MnemonicImportDialog),
    model(_model)
{
    ui->setupUi(this);

    ui->statusLabel->clear();
    connect(ui->cancelButton, &QPushButton::clicked, this, &MnemonicImportDialog::on_cancelButton_clicked);
    connect(ui->bip44CheckBox, &QCheckBox::toggled, ui->legacyCoinTypeCheckBox, &QWidget::setEnabled);
    ui->legacyCoinTypeCheckBox->setEnabled(ui->bip44CheckBox->isChecked());
}

MnemonicImportDialog::~MnemonicImportDialog()
{
    delete ui;
}

void MnemonicImportDialog::on_cancelButton_clicked()
{
    reject();
}

static void rotateBackups(const QString& backupDirAbsPath)
{
    QDir dir(backupDirAbsPath);
    if (!dir.exists()) return;

    QStringList filters;
    filters << "backup_before_mnemonic_*.dat";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    if (fileList.size() > 10) {
        for (int i = 10; i < fileList.size(); ++i) {
            QFile::remove(fileList.at(i).absoluteFilePath());
        }
    }
}

class MnemonicRescanWorker : public QRunnable
{
public:
    MnemonicRescanWorker(WalletModel* model, QObject* receiver)
        : m_model(model), m_receiver(receiver) {}

    void run() override
    {
        bool success = false;
        if (m_model) {
            success = m_model->wallet().rescanBlockchain(0);
        }
        QMetaObject::invokeMethod(m_receiver, "onRescanFinished",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, success));
    }

private:
    WalletModel* m_model;
    QObject* m_receiver;
};

void MnemonicImportDialog::onRescanFinished(bool success)
{
    if (!success) {
        QMessageBox::warning(this, tr("Rescan Failed"),
            tr("The wallet could not start or complete a blockchain rescan.\n\n"
               "If another rescan is already running, wait for it to finish or cancel it from the progress dialog, then try Tools → Rescan again."));
    }
}

void MnemonicImportDialog::on_importButton_clicked()
{
    if (!model) {
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText(tr("Error: Wallet model is not available."));
        return;
    }

    QByteArray mnemonicBytes = ui->mnemonicEdit->toPlainText().trimmed().toUtf8();
    QByteArray passphraseBytes = ui->passphraseEdit->text().toUtf8();
    SecureString mnemonicSec(mnemonicBytes.constData(), mnemonicBytes.constData() + mnemonicBytes.size());
    SecureString passphraseSec(passphraseBytes.constData(), passphraseBytes.constData() + passphraseBytes.size());

    if (mnemonicSec.empty()) {
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText(tr("Please enter your mnemonic phrase."));
        return;
    }

    std::string error_msg;
    if (!Mnemonic::Validate(mnemonicSec, error_msg)) {
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText(tr("Validation failed: %1").arg(QString::fromStdString(error_msg)));
        return;
    }

    ui->statusLabel->setStyleSheet("color: #ffa500;");
    ui->statusLabel->setText(tr("Mnemonic valid! Preparing migration and wallet backup..."));
    QCoreApplication::processEvents();

    QString dataDir = QString::fromStdString(GetDataDir().string());
    QString backupDirName = model->wallet().isLegacy() ? "wallet_backups" : "backups";
    QDir backupDir(dataDir);
    backupDir.mkpath(backupDirName);
    QString backupPath = dataDir + "/" + backupDirName + "/backup_before_mnemonic_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".dat";

    if (!model->wallet().backupWallet(backupPath.toStdString())) {
        QFileInfo backupInfo(backupPath);
        QString absoluteBackupPath = backupInfo.absoluteFilePath();

        QMessageBox::StandardButton btn = QMessageBox::warning(this, tr("Backup Directory Write Permission Denied"),
            tr("The wallet could not save the automatic backup to:\n%1\n\n"
               "This is likely due to insufficient write permissions in the directory.\n"
               "For security, you must perform a manual backup before importing. Would you like to select an alternative location to save your backup?").arg(absoluteBackupPath),
            QMessageBox::Yes | QMessageBox::No);

        if (btn == QMessageBox::Yes) {
            QString manualFilename = GUIUtil::getSaveFileName(this,
                tr("Save Backup Wallet"), QString(),
                tr("Wallet Data (*.dat)"), nullptr);

            if (manualFilename.isEmpty() || !model->wallet().backupWallet(manualFilename.toStdString())) {
                ui->statusLabel->setStyleSheet("color: #ff4500;");
                ui->statusLabel->setText(tr("Manual wallet backup failed or cancelled. Aborting import for security."));
                memory_cleanse(mnemonicSec.data(), mnemonicSec.size());
                memory_cleanse(passphraseSec.data(), passphraseSec.size());
                return;
            }
        } else {
            ui->statusLabel->setStyleSheet("color: #ff4500;");
            ui->statusLabel->setText(tr("Wallet backup failed. Aborting import for security."));
            memory_cleanse(mnemonicSec.data(), mnemonicSec.size());
            memory_cleanse(passphraseSec.data(), passphraseSec.size());
            return;
        }
    } else {
        rotateBackups(dataDir + "/" + backupDirName);
    }

    if (model->wallet().isLocked()) {
        WalletModel::UnlockContext ctx(model->requestUnlock());
        if (!ctx.isValid()) {
            ui->statusLabel->setStyleSheet("color: #ff4500;");
            ui->statusLabel->setText(tr("Wallet unlock failed or cancelled. Aborting seed import."));
            memory_cleanse(mnemonicSec.data(), mnemonicSec.size());
            memory_cleanse(passphraseSec.data(), passphraseSec.size());
            return;
        }
    }

    interfaces::MnemonicImportOptions options;
    options.use_bip44 = ui->bip44CheckBox->isChecked();
    options.bip44_coin_type = ui->legacyCoinTypeCheckBox->isChecked() ? 398u : 0u;
    options.gap_limit = 1000;

    std::vector<unsigned char> seed = Mnemonic::DeriveSeed(mnemonicSec, passphraseSec);
    memory_cleanse(mnemonicSec.data(), mnemonicSec.size());
    memory_cleanse(passphraseSec.data(), passphraseSec.size());

    if (!model->wallet().importMnemonicSeed(seed, options)) {
        memory_cleanse(seed.data(), seed.size());
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText(tr("Failed to import derived seed into core. Check if HD is enabled, if the wallet is locked, or if the keys already exist."));
        return;
    }
    memory_cleanse(seed.data(), seed.size());

    ui->statusLabel->setStyleSheet("color: #00ff00;");
    if (options.use_bip44) {
        ui->statusLabel->setText(tr("Web wallet compatible recovery complete. Starting blockchain rescan..."));
    } else {
        ui->statusLabel->setText(tr("Migration successful! Starting blockchain rescan..."));
    }
    QCoreApplication::processEvents();

    MnemonicRescanWorker* worker = new MnemonicRescanWorker(model, this);
    worker->setAutoDelete(true);
    QThreadPool::globalInstance()->start(worker);

    QString importMsg;
    if (options.use_bip44) {
        importMsg = tr("Web wallet compatible BIP44 mnemonic recovery has completed.\n\n"
                       "Keys were imported along path m/44'/%1'/0'/change/index (up to %2 addresses per chain).\n\n"
                       "A full blockchain rescan has started. Keep the wallet open until it finishes.\n"
                       "Note: proof-of-stake rewards require coins to mature for several days before minting.")
                      .arg(options.bip44_coin_type)
                      .arg(options.gap_limit);
    } else {
        importMsg = tr("Your wallet has been migrated to the BIP39 seed using the XPChain Core HD scheme (m/0'/...).\n\n"
                       "This path is different from the web wallet BIP44 layout unless you enabled web wallet recovery.\n\n"
                       "A full blockchain rescan has started in the background.");
    }

    QMessageBox::information(this, tr("Import Complete"), importMsg);
    accept();
}
