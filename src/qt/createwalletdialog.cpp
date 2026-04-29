// Copyright (c) 2019-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/createwalletdialog.h>
#include <qt/forms/ui_createwalletdialog.h>

#include <QPushButton>

CreateWalletDialog::CreateWalletDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::CreateWalletDialog)
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(ui->wallet_name, &QLineEdit::textChanged, [this](const QString& text) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
    });
}

CreateWalletDialog::~CreateWalletDialog()
{
    delete ui;
}

QString CreateWalletDialog::walletName() const
{
    return ui->wallet_name->text();
}

bool CreateWalletDialog::disablePrivateKeys() const
{
    return ui->disable_privkeys_cb->isChecked();
}

bool CreateWalletDialog::makeBlankWallet() const
{
    return ui->make_blank_cb->isChecked();
}

bool CreateWalletDialog::descriptors() const
{
    return ui->descriptor_cb->isChecked();
}
