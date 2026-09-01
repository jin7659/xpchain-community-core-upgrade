// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_WALLETSETUPDIALOG_H
#define XPCHAIN_QT_WALLETSETUPDIALOG_H

#include <QDialog>

class QAbstractButton;

/** First-run style chooser: create, generate mnemonic, restore, or open. */
class WalletSetupDialog : public QDialog
{
    Q_OBJECT

public:
    enum Choice {
        CreateEmpty,
        GenerateMnemonic,
        RestoreMnemonic,
        OpenExisting
    };

    explicit WalletSetupDialog(QWidget* parent = nullptr);

    Choice choice() const { return m_choice; }

private Q_SLOTS:
    void onChoiceClicked(QAbstractButton* button);

private:
    Choice m_choice;
    QAbstractButton* m_btnCreate;
    QAbstractButton* m_btnGenerate;
    QAbstractButton* m_btnRestore;
    QAbstractButton* m_btnOpen;
};

#endif // XPCHAIN_QT_WALLETSETUPDIALOG_H
