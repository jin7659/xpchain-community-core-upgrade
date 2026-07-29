// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_ASKPASSPHRASEDIALOG_H
#define XPCHAIN_QT_ASKPASSPHRASEDIALOG_H

#include <support/allocators/secure.h>

#include <QDialog>

class WalletModel;

namespace Ui {
    class AskPassphraseDialog;
}

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class AskPassphraseDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        Encrypt,         /**< Ask passphrase twice and encrypt */
        Unlock,          /**< Ask passphrase and unlock */
        ChangePass,      /**< Ask old passphrase + new passphrase twice */
        Decrypt,         /**< Ask passphrase and decrypt wallet */
        DatabaseUnlock   /**< Ask passphrase to open SQLCipher wallet DB (no WalletModel) */
    };

    explicit AskPassphraseDialog(Mode mode, QWidget *parent, const QString& warningText = QString());
    ~AskPassphraseDialog();

    void accept();

    void setModel(WalletModel *model);

    /** Passphrase collected in DatabaseUnlock mode. */
    const SecureString& getPassphrase() const { return m_passphrase; }

private:
    Ui::AskPassphraseDialog *ui;
    Mode mode;
    WalletModel *model;
    bool fCapsLock;
    SecureString m_passphrase;

private Q_SLOTS:
    void textChanged();
    void secureClearPassFields();
    void toggleShowPassword(bool);

protected:
    bool event(QEvent *event);
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // XPCHAIN_QT_ASKPASSPHRASEDIALOG_H
