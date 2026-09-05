// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mnemonicimportdialog.h>
#include <qt/forms/ui_mnemonicimportdialog.h>
#include <qt/walletmodel.h>
#include <qt/mnemonic.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>

#include <wallet/bip39_words.h>

#include <QMessageBox>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QThreadPool>
#include <QShowEvent>
#include <QRegularExpression>
#include <QSet>
#include <QCoreApplication>

#include <util.h>
#include <support/cleanse.h>

#include <vector>

namespace {
class MnemonicRescanWorker : public QRunnable
{
public:
    MnemonicRescanWorker(WalletModel* model) : m_model(model) {}

    void run() override
    {
        bool success = false;
        if (m_model) {
            success = m_model->wallet().rescanBlockchain(0);
        }
        if (m_model) {
            QMetaObject::invokeMethod(m_model, "notifyMnemonicRescanFinished",
                                      Qt::QueuedConnection,
                                      Q_ARG(bool, success));
        }
    }

private:
    WalletModel* m_model;
};
} // namespace

MnemonicImportDialog::MnemonicImportDialog(QWidget *parent, WalletModel *_model) :
    QDialog(parent),
    ui(new Ui::MnemonicImportDialog),
    model(_model),
    m_empty_wallet_warned(false)
{
    ui->setupUi(this);

    ui->passphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->statusLabel->clear();
    updateBackupWarning();
    onBip44Toggled(ui->bip44CheckBox->isChecked());

    connect(ui->cancelButton, &QPushButton::clicked, this, &MnemonicImportDialog::on_cancelButton_clicked);
    connect(ui->createEmptyWalletButton, &QPushButton::clicked, this, &MnemonicImportDialog::on_createEmptyWalletButton_clicked);
    connect(ui->bip44CheckBox, &QCheckBox::toggled, this, &MnemonicImportDialog::onBip44Toggled);
    connect(ui->legacyCoinTypeCheckBox, &QCheckBox::toggled, this, [this](bool) {
        onBip44Toggled(ui->bip44CheckBox->isChecked());
    });
    connect(ui->showPassphraseCheckBox, &QCheckBox::toggled, this, &MnemonicImportDialog::onShowPassphraseToggled);
    connect(ui->mnemonicEdit, &QPlainTextEdit::textChanged, this, &MnemonicImportDialog::onMnemonicTextChanged);

    onMnemonicTextChanged();
}

MnemonicImportDialog::~MnemonicImportDialog()
{
    delete ui;
}

void MnemonicImportDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!m_empty_wallet_warned) {
        m_empty_wallet_warned = true;
        warnIfWalletNotEmpty();
    }
}

void MnemonicImportDialog::on_cancelButton_clicked()
{
    reject();
}

void MnemonicImportDialog::on_createEmptyWalletButton_clicked()
{
    Q_EMIT createEmptyWalletRequested();
    reject();
}

void MnemonicImportDialog::onBip44Toggled(bool checked)
{
    ui->legacyCoinTypeCheckBox->setEnabled(checked);
    if (!checked) {
        ui->legacyCoinTypeCheckBox->setChecked(false);
        ui->pathHintLabel->setText(
            tr("Using XPChain Core HD (m/0'/…). Uncheck only for Core-native seeds — "
               "this path does not match XPChain web wallet addresses."));
    } else if (ui->legacyCoinTypeCheckBox->isChecked()) {
        ui->pathHintLabel->setText(
            tr("BIP44 path m/44'/398'/0'/… (legacy web wallet). Prefer coin_type 0 unless you know you need 398."));
    } else {
        ui->pathHintLabel->setText(
            tr("BIP44 path m/44'/0'/0'/… — current XPChain web wallet (recommended)."));
    }
}

void MnemonicImportDialog::onShowPassphraseToggled(bool show)
{
    ui->passphraseEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

const QSet<QString>& MnemonicImportDialog::bip39WordSet()
{
    static const QSet<QString> words = []() {
        QSet<QString> set;
        set.reserve(2048);
        for (int i = 0; i < 2048; ++i) {
            set.insert(QString::fromUtf8(BIP39_WORDS[i]));
        }
        return set;
    }();
    return words;
}

bool MnemonicImportDialog::isBip39Word(const QString& word)
{
    return bip39WordSet().contains(word.toLower());
}

void MnemonicImportDialog::setStatus(StatusKind kind, const QString& text)
{
    // Avoid neon hardcoded colors that fight the global theme; use readable accents.
    QString color;
    switch (kind) {
    case StatusError: color = QStringLiteral("#c62828"); break;
    case StatusWarn:  color = QStringLiteral("#ef6c00"); break;
    case StatusOk:    color = QStringLiteral("#2e7d32"); break;
    case StatusInfo:  color = QStringLiteral("#1565c0"); break;
    case StatusNeutral:
    default:          color = QString(); break;
    }
    if (color.isEmpty()) {
        ui->statusLabel->setStyleSheet(QString());
    } else {
        ui->statusLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(color));
    }
    ui->statusLabel->setText(text);
}

QString MnemonicImportDialog::backupDirName() const
{
    if (model && !model->wallet().isLegacy()) {
        return QStringLiteral("backups");
    }
    return QStringLiteral("wallet_backups");
}

void MnemonicImportDialog::updateBackupWarning()
{
    const QString dir = backupDirName();
    ui->warningLabel->setText(
        tr("WARNING: Import merges keys into the <b>current</b> wallet "
           "(“%1”). A backup is written to <code>%2/backup_before_mnemonic_*.dat</code> "
           "before import.<br/><br/>"
           "For a clean recovery, create a new empty wallet first "
           "(button below, or File → Set Up Wallet / File → Advanced → Create Wallet (Descriptor unchecked)), then restore into that wallet.")
            .arg(model ? QString::fromStdString(model->wallet().getWalletName()) : tr("(unknown)"))
            .arg(dir));
}

bool MnemonicImportDialog::walletLooksUsed() const
{
    if (!model) return false;
    const interfaces::WalletBalances bal = model->wallet().getBalances();
    if (bal.balance != 0 || bal.unconfirmed_balance != 0 || bal.immature_balance != 0) {
        return true;
    }
    return !model->wallet().getWalletTxs().empty();
}

void MnemonicImportDialog::warnIfWalletNotEmpty()
{
    if (!walletLooksUsed()) {
        return;
    }
    const auto btn = QMessageBox::warning(this, tr("Current wallet is not empty"),
        tr("This wallet already has a balance or transaction history.\n\n"
           "Mnemonic import merges derived keys into this wallet; it does not replace it.\n\n"
           "For a clean recovery, cancel and create a new empty wallet first.\n\n"
           "Continue merging into “%1”?")
            .arg(QString::fromStdString(model->wallet().getWalletName())),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (btn != QMessageBox::Yes) {
        reject();
    }
}

void MnemonicImportDialog::onMnemonicTextChanged()
{
    const QString text = ui->mnemonicEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        ui->wordCountLabel->setText(tr("0 words — enter 12 or 24 BIP39 words"));
        setStatus(StatusNeutral, QString());
        return;
    }

    const QStringList words = text.split(QRegularExpression(QStringLiteral("\\s+")), QString::SkipEmptyParts);
    const int n = words.size();
    QStringList invalid;
    for (const QString& word : words) {
        if (!isBip39Word(word)) {
            invalid.append(word);
        }
    }

    QString count_text = tr("%1 word(s)").arg(n);
    const bool length_ok = (n == 12 || n == 15 || n == 18 || n == 21 || n == 24);
    if (length_ok) {
        count_text += tr(" · length OK");
    } else {
        count_text += tr(" · need 12 or 24 (also accepts 15/18/21)");
    }

    if (!invalid.isEmpty()) {
        const QString shown = invalid.mid(0, 4).join(QStringLiteral(", "));
        count_text += tr(" · not in BIP39 list: %1").arg(shown);
        if (invalid.size() > 4) {
            count_text += tr("…");
        }
        ui->wordCountLabel->setText(count_text);
        setStatus(StatusWarn, tr("Fix unknown words before importing."));
        return;
    }

    // Suggest completions for the last incomplete token (still typing).
    const bool ends_with_space = ui->mnemonicEdit->toPlainText().endsWith(QLatin1Char(' '))
        || ui->mnemonicEdit->toPlainText().endsWith(QLatin1Char('\n'));
    if (!ends_with_space && !words.isEmpty()) {
        const QString partial = words.last().toLower();
        if (partial.size() >= 2 && !bip39WordSet().contains(partial)) {
            QStringList suggestions;
            for (const QString& candidate : bip39WordSet()) {
                if (candidate.startsWith(partial)) {
                    suggestions.append(candidate);
                    if (suggestions.size() >= 5) break;
                }
            }
            if (!suggestions.isEmpty()) {
                count_text += tr(" · suggestions: %1").arg(suggestions.join(QStringLiteral(", ")));
            }
        }
    }

    ui->wordCountLabel->setText(count_text);
    if (length_ok) {
        setStatus(StatusInfo, tr("Word list looks valid. Full checksum is checked on Import."));
    } else {
        setStatus(StatusNeutral, QString());
    }
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

void MnemonicImportDialog::on_importButton_clicked()
{
    if (!model) {
        setStatus(StatusError, tr("Error: Wallet model is not available."));
        return;
    }

    if (model->wallet().isDescriptor()) {
        setStatus(StatusError, tr(
            "This is a descriptor wallet. BIP39 mnemonic restore needs a legacy HD wallet.
"
            "Create an empty non-descriptor wallet first (Create empty wallet / File → Advanced → Create Wallet with Descriptor unchecked), then restore into it."));
        return;
    }

    QByteArray mnemonicBytes = ui->mnemonicEdit->toPlainText().trimmed().toUtf8();
    QByteArray passphraseBytes = ui->passphraseEdit->text().toUtf8();
    SecureString mnemonicSec(mnemonicBytes.constData(), mnemonicBytes.constData() + mnemonicBytes.size());
    SecureString passphraseSec(passphraseBytes.constData(), passphraseBytes.constData() + passphraseBytes.size());

    if (mnemonicSec.empty()) {
        setStatus(StatusError, tr("Please enter your mnemonic phrase."));
        return;
    }

    std::string error_msg;
    if (!Mnemonic::Validate(mnemonicSec, error_msg)) {
        setStatus(StatusError, tr("Validation failed: %1").arg(QString::fromStdString(error_msg)));
        return;
    }

    setStatus(StatusWarn, tr("Mnemonic valid. Creating wallet backup…"));
    QCoreApplication::processEvents();

    QString dataDir = QString::fromStdString(GetDataDir().string());
    QString backupDir = backupDirName();
    QDir(dataDir).mkpath(backupDir);
    QString backupPath = dataDir + "/" + backupDir + "/backup_before_mnemonic_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".dat";

    if (!model->wallet().backupWallet(backupPath.toStdString())) {
        QFileInfo backupInfo(backupPath);
        QString absoluteBackupPath = backupInfo.absoluteFilePath();

        QMessageBox::StandardButton btn = QMessageBox::warning(this, tr("Backup Directory Write Permission Denied"),
            tr("The wallet could not save the automatic backup to:\n%1\n\n"
               "This is likely due to insufficient write permissions in the directory.\n"
               "For security, you must perform a manual backup before importing. Would you like to select an alternative location to save your backup?")
                .arg(absoluteBackupPath),
            QMessageBox::Yes | QMessageBox::No);

        if (btn == QMessageBox::Yes) {
            QString manualFilename = GUIUtil::getSaveFileName(this,
                tr("Save Backup Wallet"), QString(),
                tr("Wallet Data (*.dat)"), nullptr);

            if (manualFilename.isEmpty() || !model->wallet().backupWallet(manualFilename.toStdString())) {
                setStatus(StatusError, tr("Manual wallet backup failed or cancelled. Aborting import for security."));
                memory_cleanse(mnemonicSec.data(), mnemonicSec.size());
                memory_cleanse(passphraseSec.data(), passphraseSec.size());
                return;
            }
        } else {
            setStatus(StatusError, tr("Wallet backup failed. Aborting import for security."));
            memory_cleanse(mnemonicSec.data(), mnemonicSec.size());
            memory_cleanse(passphraseSec.data(), passphraseSec.size());
            return;
        }
    } else {
        rotateBackups(dataDir + "/" + backupDir);
    }

    if (model->wallet().isLocked()) {
        WalletModel::UnlockContext ctx(model->requestUnlock());
        if (!ctx.isValid()) {
            setStatus(StatusError, tr("Wallet unlock failed or cancelled. Aborting seed import."));
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
    ui->mnemonicEdit->clear();
    ui->passphraseEdit->clear();

    if (!model->wallet().importMnemonicSeed(seed, options)) {
        memory_cleanse(seed.data(), seed.size());
        setStatus(StatusError, tr("Failed to import derived seed. Check that HD is enabled, the wallet is unlocked, and keys are not conflicting."));
        return;
    }
    memory_cleanse(seed.data(), seed.size());

    setStatus(StatusOk, tr("Keys imported. Starting blockchain rescan — progress appears in the main window."));
    QCoreApplication::processEvents();

    MnemonicRescanWorker* worker = new MnemonicRescanWorker(model);
    worker->setAutoDelete(true);
    QThreadPool::globalInstance()->start(worker);

    QString importMsg;
    if (options.use_bip44) {
        importMsg = tr("Web wallet compatible BIP44 mnemonic recovery has completed.\n\n"
                       "Keys were imported along path m/44'/%1'/0'/change/index (up to %2 addresses per chain).\n\n"
                       "A full blockchain rescan has started. Keep the wallet open until the progress dialog finishes.\n"
                       "You will get a notification when the rescan completes.\n\n"
                       "Note: proof-of-stake rewards require coins to mature for several days before staking.")
                      .arg(options.bip44_coin_type)
                      .arg(options.gap_limit);
    } else {
        importMsg = tr("Your wallet has been migrated to the BIP39 seed using the XPChain Core HD scheme (m/0'/...).\n\n"
                       "This path is different from the web wallet BIP44 layout.\n\n"
                       "A full blockchain rescan has started. Keep the wallet open until it finishes; "
                       "a notification will appear when it completes.");
    }

    QMessageBox::information(this, tr("Import Complete"), importMsg);
    accept();
}
