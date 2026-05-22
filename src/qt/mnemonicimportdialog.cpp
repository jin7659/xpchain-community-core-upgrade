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
#include <qt/guiutil.h>

#include <thread>
#include <vector>

MnemonicImportDialog::MnemonicImportDialog(QWidget *parent, WalletModel *_model) :
    QDialog(parent),
    ui(new Ui::MnemonicImportDialog),
    model(_model)
{
    ui->setupUi(this);
    
    // UI elements default styling
    ui->statusLabel->clear();
    
    // Connect cancelButton
    connect(ui->cancelButton, &QPushButton::clicked, this, &MnemonicImportDialog::on_cancelButton_clicked);
}

MnemonicImportDialog::~MnemonicImportDialog()
{
    delete ui;
}

void MnemonicImportDialog::on_cancelButton_clicked()
{
    reject();
}

void MnemonicImportDialog::on_importButton_clicked()
{
    if (!model) {
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText("Error: Wallet model is not available.");
        return;
    }

    QString mnemonicText = ui->mnemonicEdit->toPlainText().trimmed();
    std::string mnemonic = mnemonicText.toStdString();
    std::string passphrase = ui->passphraseEdit->text().toStdString();

    if (mnemonic.empty()) {
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText("Please enter your mnemonic phrase.");
        return;
    }

    // 1. Validate mnemonic
    std::string error_msg;
    if (!Mnemonic::Validate(mnemonic, error_msg)) {
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText(QString::fromStdString("Validation failed: " + error_msg));
        return;
    }

    ui->statusLabel->setStyleSheet("color: #ffa500;");
    ui->statusLabel->setText("Mnemonic valid! Preparing migration and wallet backup...");
    QCoreApplication::processEvents();

    // 2. Perform wallet backup
    // Create backup directory inside data dir
    QString backupDir = model->wallet().isLegacy() ? "wallet_backups" : "backups";
    QDir().mkpath(backupDir);
    QString backupPath = backupDir + "/backup_before_mnemonic_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".dat";
    
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
                ui->statusLabel->setText("Manual wallet backup failed or cancelled. Aborting import for security.");
                return;
            }
        } else {
            ui->statusLabel->setStyleSheet("color: #ff4500;");
            ui->statusLabel->setText("Wallet backup failed. Aborting import for security.");
            return;
        }
    }


    // 3. Derive 64-byte seed and import into wallet
    // 지갑이 암호화되어 잠겨 있는 경우, 안전하게 완전 잠금 해제(Full Unlock)를 자동 유도
    if (model->wallet().isLocked()) {
        WalletModel::UnlockContext ctx(model->requestUnlock());
        if (!ctx.isValid()) {
            ui->statusLabel->setStyleSheet("color: #ff4500;");
            ui->statusLabel->setText("Wallet unlock failed or cancelled. Aborting seed import.");
            return;
        }
    }

    std::vector<unsigned char> seed = Mnemonic::DeriveSeed(mnemonic, passphrase);
    
    if (!model->wallet().importMnemonicSeed(seed)) {
        ui->statusLabel->setStyleSheet("color: #ff4500;");
        ui->statusLabel->setText("Failed to import derived seed into core. Check if HD is enabled or if the wallet is locked.");
        return;
    }

    ui->statusLabel->setStyleSheet("color: #00ff00;");
    ui->statusLabel->setText("Migration successful! Triggering full blockchain rescan...");
    QCoreApplication::processEvents();

    // 4. Trigger asynchronous blockchain rescan
    WalletModel *walletModel = model;
    std::thread([walletModel]() {
        // Start scanning from genesis block (0) to discover all historical assets
        walletModel->wallet().rescanFromTime(0);
    }).detach();

    QMessageBox::information(this, tr("Import Complete"),
        tr("Your wallet has been successfully migrated to the BIP39 mnemonic seed!\n\n"
           "A full blockchain rescan has been started in the background to discover historical transactions and restore your asset balances.\n\n"
           "This process may take some time depending on your node sync state. Please leave the wallet running until completed."));

    accept();
}
