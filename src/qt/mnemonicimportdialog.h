// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_MNEMONICIMPORTDIALOG_H
#define XPCHAIN_QT_MNEMONICIMPORTDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
class MnemonicImportDialog;
}

class MnemonicImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MnemonicImportDialog(QWidget *parent = nullptr, WalletModel *model = nullptr);
    ~MnemonicImportDialog();

private Q_SLOTS:
    void on_importButton_clicked();
    void on_cancelButton_clicked();

private:
    Ui::MnemonicImportDialog *ui;
    WalletModel *model;
};

#endif // XPCHAIN_QT_MNEMONICIMPORTDIALOG_H
