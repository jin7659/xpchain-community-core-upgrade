// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/migratewalletdialog.h>
#include <qt/forms/ui_migratewalletdialog.h>
#include <qt/walletmodel.h>

#include <QMessageBox>
#include <QPushButton>

MigrateWalletDialog::MigrateWalletDialog(QWidget* parent, WalletModel* _model) :
    QDialog(parent),
    ui(new Ui::MigrateWalletDialog),
    model(_model),
    m_can_migrate(false)
{
    ui->setupUi(this);

    QString name = tr("(unknown)");
    if (model) {
        name = QString::fromStdString(model->wallet().getWalletName());
        if (name.isEmpty()) name = tr("default");
        m_can_migrate = model->isLegacy();
    }

    if (m_can_migrate) {
        ui->walletInfoLabel->setText(
            tr("<b>Current wallet:</b> %1<br/>"
               "<b>Format:</b> Berkeley DB (legacy)<br/><br/>"
               "A new SQLite wallet file will be created. The original file is not deleted.")
                .arg(name.toHtmlEscaped()));
        const QString default_dest = name + QStringLiteral(".sqlite");
        ui->destinationEdit->setText(default_dest);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Migrate"));
    } else {
        ui->walletInfoLabel->setText(
            tr("<b>Current wallet:</b> %1<br/>"
               "<b>Format:</b> SQLite (or already non-Berkeley)<br/><br/>"
               "Migration is only available for Berkeley DB wallets. "
               "This wallet does not need to be migrated.")
                .arg(name.toHtmlEscaped()));
        ui->destinationEdit->setEnabled(false);
        ui->backupCheckBox->setEnabled(false);
        ui->loadNewCheckBox->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }

    connect(ui->destinationEdit, &QLineEdit::textChanged, this, &MigrateWalletDialog::updateOkButton);
    updateOkButton();
}

MigrateWalletDialog::~MigrateWalletDialog()
{
    delete ui;
}

bool MigrateWalletDialog::canMigrate() const
{
    return m_can_migrate;
}

QString MigrateWalletDialog::destination() const
{
    return ui->destinationEdit->text().trimmed();
}

bool MigrateWalletDialog::doBackup() const
{
    return ui->backupCheckBox->isChecked();
}

bool MigrateWalletDialog::loadNew() const
{
    return ui->loadNewCheckBox->isChecked();
}

void MigrateWalletDialog::updateOkButton()
{
    if (!m_can_migrate) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }
    const QString dest = destination();
    const bool ok = !dest.isEmpty() && dest.endsWith(QStringLiteral(".sqlite"), Qt::CaseInsensitive);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

void MigrateWalletDialog::accept()
{
    if (!m_can_migrate) {
        return;
    }
    const QString dest = destination();
    if (dest.isEmpty() || !dest.endsWith(QStringLiteral(".sqlite"), Qt::CaseInsensitive)) {
        QMessageBox::warning(this, tr("Invalid destination"),
                             tr("Destination must be a non-empty path ending in .sqlite."));
        return;
    }

    const auto btn = QMessageBox::question(this, tr("Confirm wallet migration"),
        tr("Migrate this Berkeley DB wallet to SQLite?\n\n"
           "Destination: %1\n"
           "Backup source: %2\n"
           "Switch after migration: %3\n\n"
           "The original wallet file will be kept. Verify balances after migration.")
            .arg(dest)
            .arg(doBackup() ? tr("Yes") : tr("No"))
            .arg(loadNew() ? tr("Yes") : tr("No")),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (btn != QMessageBox::Yes) {
        return;
    }
    QDialog::accept();
}
