// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_MIGRATEWALLETDIALOG_H
#define XPCHAIN_QT_MIGRATEWALLETDIALOG_H

#include <QDialog>
#include <QString>

class WalletModel;

namespace Ui {
class MigrateWalletDialog;
}

/** Dialog to migrate a Berkeley DB wallet to SQLite via migratewallet RPC. */
class MigrateWalletDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MigrateWalletDialog(QWidget* parent, WalletModel* model);
    ~MigrateWalletDialog();

    void accept() override;

    bool canMigrate() const;
    QString destination() const;
    bool doBackup() const;
    bool loadNew() const;
    bool inPlace() const;
    bool overwrite() const;

private Q_SLOTS:
    void updateOkButton();
    void onReplaceSourceToggled(bool checked);

private:
    Ui::MigrateWalletDialog* ui;
    WalletModel* model;
    bool m_can_migrate;
};

#endif // XPCHAIN_QT_MIGRATEWALLETDIALOG_H
