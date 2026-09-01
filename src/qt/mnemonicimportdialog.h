// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_MNEMONICIMPORTDIALOG_H
#define XPCHAIN_QT_MNEMONICIMPORTDIALOG_H

#include <QDialog>
#include <QSet>
#include <QString>

class WalletModel;
class QShowEvent;

namespace Ui {
class MnemonicImportDialog;
}

class MnemonicImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MnemonicImportDialog(QWidget *parent = nullptr, WalletModel *model = nullptr);
    ~MnemonicImportDialog();

Q_SIGNALS:
    /** User asked to create an empty wallet before restoring. */
    void createEmptyWalletRequested();

protected:
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
    void on_importButton_clicked();
    void on_cancelButton_clicked();
    void on_createEmptyWalletButton_clicked();
    void onMnemonicTextChanged();
    void onBip44Toggled(bool checked);
    void onShowPassphraseToggled(bool checked);

private:
    enum StatusKind {
        StatusNeutral,
        StatusInfo,
        StatusWarn,
        StatusError,
        StatusOk
    };

    void setStatus(StatusKind kind, const QString& text);
    void updateBackupWarning();
    void warnIfWalletNotEmpty();
    bool walletLooksUsed() const;
    QString backupDirName() const;
    static bool isBip39Word(const QString& word);
    static const QSet<QString>& bip39WordSet();

    Ui::MnemonicImportDialog *ui;
    WalletModel *model;
    bool m_empty_wallet_warned;
};

#endif // XPCHAIN_QT_MNEMONICIMPORTDIALOG_H
