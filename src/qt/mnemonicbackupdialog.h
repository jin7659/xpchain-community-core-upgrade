// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_MNEMONICBACKUPDIALOG_H
#define XPCHAIN_QT_MNEMONICBACKUPDIALOG_H

#include <support/allocators/secure.h>

#include <QDialog>
#include <QString>

namespace Ui {
class MnemonicBackupDialog;
}

/** Wizard: generate BIP39 mnemonic, confirm backup, then create a seeded wallet. */
class MnemonicBackupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MnemonicBackupDialog(QWidget* parent = nullptr);
    ~MnemonicBackupDialog();

    QString walletName() const;
    bool encryptWallet() const;
    QString walletPassphrase() const;
    bool useBip44() const;
    unsigned int bip44CoinType() const;
    QString bip39Passphrase() const;
    /** Confirmed mnemonic phrase (only valid after accept). */
    SecureString mnemonic() const;

    void secureClearSecrets();

protected:
    void reject() override;

private Q_SLOTS:
    void onBackClicked();
    void onNextClicked();
    void onEncryptToggled(bool enabled);
    void onBip44Toggled(bool enabled);
    void onShowWalletPassToggled(bool show);
    void onShowBip39PassToggled(bool show);
    void onShowMnemonicToggled(bool show);
    void onWroteDownToggled(bool checked);
    void updateSetupNextEnabled();
    void updateConfirmNextEnabled();

private:
    enum Page {
        PageSetup = 0,
        PageShow = 1,
        PageConfirm = 2
    };

    void setPage(int page);
    bool validateSetup();
    bool generateAndShowMnemonic();
    bool validateConfirm();
    void refreshMnemonicDisplay();
    static QString normalizeMnemonic(const QString& text);

    Ui::MnemonicBackupDialog* ui;
    SecureString m_mnemonic;
    bool m_mnemonic_visible;
};

#endif // XPCHAIN_QT_MNEMONICBACKUPDIALOG_H
