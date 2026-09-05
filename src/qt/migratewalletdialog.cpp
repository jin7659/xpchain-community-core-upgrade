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
        ui->replaceSourceCheckBox->setEnabled(false);
        ui->overwriteCheckBox->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }

    // In-process wallet swap after migrate is unsafe; always restart instead.
    ui->loadNewCheckBox->setChecked(false);
    ui->loadNewCheckBox->setEnabled(false);
    ui->loadNewCheckBox->setText(tr("XPChain will quit after migration so you can restart with SQLite (required)."));
    ui->loadNewCheckBox->setToolTip(tr(
        "Loading the migrated wallet in this same process is disabled to avoid Berkeley DB / staking thread issues. "
        "XPChain quits after a successful migration; reopen the app to use the SQLite wallet."));

    connect(ui->replaceSourceCheckBox, &QCheckBox::toggled, this, &MigrateWalletDialog::onReplaceSourceToggled);
    connect(ui->destinationEdit, &QLineEdit::textChanged, this, &MigrateWalletDialog::updateOkButton);
    connect(ui->overwriteCheckBox, &QCheckBox::toggled, this, &MigrateWalletDialog::updateOkButton);
    onReplaceSourceToggled(ui->replaceSourceCheckBox->isChecked());
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

bool MigrateWalletDialog::inPlace() const
{
    return ui->replaceSourceCheckBox->isChecked();
}

bool MigrateWalletDialog::overwrite() const
{
    return ui->overwriteCheckBox->isChecked();
}

void MigrateWalletDialog::onReplaceSourceToggled(bool checked)
{
    ui->destinationEdit->setEnabled(!checked);
    ui->overwriteCheckBox->setEnabled(!checked);
    if (checked) {
        ui->summaryLabel->setText(
            tr("Upgrade this wallet to SQLite in place. The original Berkeley DB file is kept as a .legacy.bak backup."));
        ui->destinationHintLabel->setText(
            tr("The current wallet will be upgraded to SQLite in-place, and the original will be backed up as .legacy.bak.<br/>"
               "<b>This guarantees SQLite is automatically recognized on next start.</b>"));
    } else {
        ui->summaryLabel->setText(
            tr("Copy this wallet into a new SQLite file. The original Berkeley DB file is kept."));
        ui->destinationHintLabel->setText(
            tr("Must end with .sqlite. Relative paths are under the wallets data directory."));
    }
    updateOkButton();
}

void MigrateWalletDialog::updateOkButton()
{
    if (!m_can_migrate) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }
    if (inPlace()) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
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
    if (!inPlace()) {
        const QString dest = destination();
        if (dest.isEmpty() || !dest.endsWith(QStringLiteral(".sqlite"), Qt::CaseInsensitive)) {
            QMessageBox::warning(this, tr("Invalid destination"),
                                 tr("Destination must be a non-empty path ending in .sqlite."));
            return;
        }
    }

    const auto btn = QMessageBox::question(this, tr("Confirm wallet migration"),
        tr("Migrate this Berkeley DB wallet to SQLite?\n\n"
           "Mode: %1\n"
           "Backup source: %2\n\n"
           "The original wallet file will be safely kept as a backup. Verify balances after migration.")
            .arg(inPlace() ? tr("In-place upgrade (auto-load SQLite on restart)") : destination())
            .arg(doBackup() ? tr("Yes") : tr("No")),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (btn != QMessageBox::Yes) {
        return;
    }
    QDialog::accept();
}
