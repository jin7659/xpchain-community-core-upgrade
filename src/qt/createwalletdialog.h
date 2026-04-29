// Copyright (c) 2019-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CREATEWALLETDIALOG_H
#define BITCOIN_QT_CREATEWALLETDIALOG_H

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

    QString walletName() const;
    bool disablePrivateKeys() const;
    bool makeBlankWallet() const;
    bool descriptors() const;

private:
    Ui::CreateWalletDialog *ui;
};

#endif // BITCOIN_QT_CREATEWALLETDIALOG_H
