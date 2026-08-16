// Copyright (c) 2019-2021 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_CREATEWALLETDIALOG_H
#define XPCHAIN_QT_CREATEWALLETDIALOG_H

#include <QDialog>
#include <QString>

class WalletModel;

namespace Ui {
    class CreateWalletDialog;
}

/** Dialog for creating a new wallet. */
class CreateWalletDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateWalletDialog(QWidget* parent);
    virtual ~CreateWalletDialog();

    void accept() override;

    QString walletName() const;
    bool disablePrivateKeys() const;
    bool descriptors() const;
    bool encryptWallet() const;
    /** Passphrase when encryptWallet() is true; empty otherwise. Cleared after read in createWallet(). */
    QString passphrase() const;
    void secureClearPassphrases();

private Q_SLOTS:
    void updateOkButton();
    void setEncryptionWidgetsEnabled(bool enabled);
    void toggleShowPassphrase(bool show);

private:
    Ui::CreateWalletDialog *ui;
};

#endif // XPCHAIN_QT_CREATEWALLETDIALOG_H
