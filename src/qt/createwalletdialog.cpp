// Copyright (c) 2019-2021 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/createwalletdialog.h>
#include <qt/forms/ui_createwalletdialog.h>
#include <qt/guiconstants.h>

#include <QMessageBox>
#include <QPushButton>

CreateWalletDialog::CreateWalletDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::CreateWalletDialog)
{
    ui->setupUi(this);

    ui->passphrase_edit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->confirm_edit->setMaxLength(MAX_PASSPHRASE_SIZE);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(ui->wallet_name, &QLineEdit::textChanged, this, &CreateWalletDialog::updateOkButton);
    connect(ui->encrypt_wallet_cb, &QCheckBox::toggled, this, &CreateWalletDialog::setEncryptionWidgetsEnabled);
    connect(ui->passphrase_edit, &QLineEdit::textChanged, this, &CreateWalletDialog::updateOkButton);
    connect(ui->confirm_edit, &QLineEdit::textChanged, this, &CreateWalletDialog::updateOkButton);
    connect(ui->show_passphrase_cb, &QCheckBox::toggled, this, &CreateWalletDialog::toggleShowPassphrase);

    setEncryptionWidgetsEnabled(ui->encrypt_wallet_cb->isChecked());
    updateOkButton();
}

CreateWalletDialog::~CreateWalletDialog()
{
    secureClearPassphrases();
    delete ui;
}

void CreateWalletDialog::setEncryptionWidgetsEnabled(bool enabled)
{
    ui->passphrase_label->setEnabled(enabled);
    ui->confirm_label->setEnabled(enabled);
    ui->passphrase_edit->setEnabled(enabled);
    ui->confirm_edit->setEnabled(enabled);
    ui->show_passphrase_cb->setEnabled(enabled);
    ui->encrypt_hint_label->setVisible(enabled);
    if (!enabled) {
        secureClearPassphrases();
    }
    updateOkButton();
}

void CreateWalletDialog::toggleShowPassphrase(bool show)
{
    const auto mode = show ? QLineEdit::Normal : QLineEdit::Password;
    ui->passphrase_edit->setEchoMode(mode);
    ui->confirm_edit->setEchoMode(mode);
}

void CreateWalletDialog::updateOkButton()
{
    bool ok = !ui->wallet_name->text().trimmed().isEmpty();
    if (ui->encrypt_wallet_cb->isChecked()) {
        ok = ok && !ui->passphrase_edit->text().isEmpty() && !ui->confirm_edit->text().isEmpty();
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

void CreateWalletDialog::accept()
{
    if (ui->encrypt_wallet_cb->isChecked()) {
        if (ui->passphrase_edit->text() != ui->confirm_edit->text()) {
            QMessageBox::critical(this, tr("Wallet creation failed"),
                                  tr("The supplied passphrases do not match."));
            return;
        }
        if (ui->passphrase_edit->text().isEmpty()) {
            QMessageBox::critical(this, tr("Wallet creation failed"),
                                  tr("Enter a passphrase to encrypt the wallet, or uncheck Encrypt wallet."));
            return;
        }
    }
    QDialog::accept();
}

QString CreateWalletDialog::walletName() const
{
    return ui->wallet_name->text().trimmed();
}

bool CreateWalletDialog::disablePrivateKeys() const
{
    return ui->disable_privkeys_cb->isChecked();
}

bool CreateWalletDialog::descriptors() const
{
    return ui->descriptor_cb->isChecked();
}

bool CreateWalletDialog::encryptWallet() const
{
    return ui->encrypt_wallet_cb->isChecked();
}

QString CreateWalletDialog::passphrase() const
{
    if (!encryptWallet()) {
        return QString();
    }
    return ui->passphrase_edit->text();
}

void CreateWalletDialog::secureClearPassphrases()
{
    auto clear = [](QLineEdit* edit) {
        edit->setText(QString(" ").repeated(edit->text().size()));
        edit->clear();
    };
    clear(ui->passphrase_edit);
    clear(ui->confirm_edit);
}
