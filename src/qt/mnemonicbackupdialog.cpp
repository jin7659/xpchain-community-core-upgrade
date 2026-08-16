// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mnemonicbackupdialog.h>
#include <qt/forms/ui_mnemonicbackupdialog.h>
#include <qt/guiconstants.h>
#include <qt/mnemonic.h>

#include <support/cleanse.h>

#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>

#include <stdexcept>
#include <string>

MnemonicBackupDialog::MnemonicBackupDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::MnemonicBackupDialog),
    m_mnemonic_visible(true)
{
    ui->setupUi(this);

    ui->passphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->confirmEdit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->bip39PassEdit->setMaxLength(MAX_PASSPHRASE_SIZE);

    connect(ui->cancelButton, &QPushButton::clicked, this, &MnemonicBackupDialog::reject);
    connect(ui->backButton, &QPushButton::clicked, this, &MnemonicBackupDialog::onBackClicked);
    connect(ui->nextButton, &QPushButton::clicked, this, &MnemonicBackupDialog::onNextClicked);
    connect(ui->encryptWalletCheckBox, &QCheckBox::toggled, this, &MnemonicBackupDialog::onEncryptToggled);
    connect(ui->bip44CheckBox, &QCheckBox::toggled, this, &MnemonicBackupDialog::onBip44Toggled);
    connect(ui->showWalletPassCheckBox, &QCheckBox::toggled, this, &MnemonicBackupDialog::onShowWalletPassToggled);
    connect(ui->showBip39PassCheckBox, &QCheckBox::toggled, this, &MnemonicBackupDialog::onShowBip39PassToggled);
    connect(ui->showMnemonicCheckBox, &QCheckBox::toggled, this, &MnemonicBackupDialog::onShowMnemonicToggled);
    connect(ui->wroteDownCheckBox, &QCheckBox::toggled, this, &MnemonicBackupDialog::onWroteDownToggled);
    connect(ui->walletNameEdit, &QLineEdit::textChanged, this, &MnemonicBackupDialog::updateSetupNextEnabled);
    connect(ui->passphraseEdit, &QLineEdit::textChanged, this, &MnemonicBackupDialog::updateSetupNextEnabled);
    connect(ui->confirmEdit, &QLineEdit::textChanged, this, &MnemonicBackupDialog::updateSetupNextEnabled);
    connect(ui->confirmMnemonicEdit, &QPlainTextEdit::textChanged, this, &MnemonicBackupDialog::updateConfirmNextEnabled);

    onEncryptToggled(ui->encryptWalletCheckBox->isChecked());
    onBip44Toggled(ui->bip44CheckBox->isChecked());
    setPage(PageSetup);
}

MnemonicBackupDialog::~MnemonicBackupDialog()
{
    secureClearSecrets();
    delete ui;
}

void MnemonicBackupDialog::reject()
{
    secureClearSecrets();
    QDialog::reject();
}

void MnemonicBackupDialog::secureClearSecrets()
{
    if (!m_mnemonic.empty()) {
        memory_cleanse(m_mnemonic.data(), m_mnemonic.size());
        m_mnemonic.clear();
    }
    auto clearLine = [](QLineEdit* edit) {
        edit->setText(QString(QChar(' ')).repeated(edit->text().size()));
        edit->clear();
    };
    clearLine(ui->passphraseEdit);
    clearLine(ui->confirmEdit);
    clearLine(ui->bip39PassEdit);
    ui->mnemonicDisplay->clear();
    ui->confirmMnemonicEdit->clear();
}

QString MnemonicBackupDialog::walletName() const
{
    return ui->walletNameEdit->text().trimmed();
}

bool MnemonicBackupDialog::encryptWallet() const
{
    return ui->encryptWalletCheckBox->isChecked();
}

QString MnemonicBackupDialog::walletPassphrase() const
{
    if (!encryptWallet()) {
        return QString();
    }
    return ui->passphraseEdit->text();
}

bool MnemonicBackupDialog::useBip44() const
{
    return ui->bip44CheckBox->isChecked();
}

unsigned int MnemonicBackupDialog::bip44CoinType() const
{
    return ui->legacyCoinTypeCheckBox->isChecked() ? 398u : 0u;
}

QString MnemonicBackupDialog::bip39Passphrase() const
{
    return ui->bip39PassEdit->text();
}

SecureString MnemonicBackupDialog::mnemonic() const
{
    return m_mnemonic;
}

void MnemonicBackupDialog::setPage(int page)
{
    ui->stackedWidget->setCurrentIndex(page);
    ui->backButton->setEnabled(page > PageSetup);
    if (page == PageConfirm) {
        ui->nextButton->setText(tr("Create Wallet"));
        updateConfirmNextEnabled();
    } else if (page == PageShow) {
        ui->nextButton->setText(tr("Next"));
        ui->nextButton->setEnabled(ui->wroteDownCheckBox->isChecked());
    } else {
        ui->nextButton->setText(tr("Next"));
        updateSetupNextEnabled();
    }
}

void MnemonicBackupDialog::onBackClicked()
{
    const int page = ui->stackedWidget->currentIndex();
    if (page == PageConfirm) {
        setPage(PageShow);
    } else if (page == PageShow) {
        // Regenerating on next forward; clear previous phrase.
        if (!m_mnemonic.empty()) {
            memory_cleanse(m_mnemonic.data(), m_mnemonic.size());
            m_mnemonic.clear();
        }
        ui->mnemonicDisplay->clear();
        ui->wroteDownCheckBox->setChecked(false);
        setPage(PageSetup);
    }
}

void MnemonicBackupDialog::onNextClicked()
{
    const int page = ui->stackedWidget->currentIndex();
    if (page == PageSetup) {
        if (!validateSetup()) {
            return;
        }
        if (!generateAndShowMnemonic()) {
            return;
        }
        setPage(PageShow);
    } else if (page == PageShow) {
        if (!ui->wroteDownCheckBox->isChecked()) {
            return;
        }
        ui->confirmMnemonicEdit->clear();
        ui->confirmStatusLabel->clear();
        setPage(PageConfirm);
    } else if (page == PageConfirm) {
        if (!validateConfirm()) {
            return;
        }
        QDialog::accept();
    }
}

bool MnemonicBackupDialog::validateSetup()
{
    ui->setupStatusLabel->clear();
    if (walletName().isEmpty()) {
        ui->setupStatusLabel->setStyleSheet(QStringLiteral("color: #c62828; font-weight: 600;"));
        ui->setupStatusLabel->setText(tr("Enter a wallet name."));
        return false;
    }
    if (encryptWallet()) {
        if (ui->passphraseEdit->text().isEmpty()) {
            ui->setupStatusLabel->setStyleSheet(QStringLiteral("color: #c62828; font-weight: 600;"));
            ui->setupStatusLabel->setText(tr("Enter a wallet passphrase, or uncheck Encrypt wallet."));
            return false;
        }
        if (ui->passphraseEdit->text() != ui->confirmEdit->text()) {
            ui->setupStatusLabel->setStyleSheet(QStringLiteral("color: #c62828; font-weight: 600;"));
            ui->setupStatusLabel->setText(tr("Wallet passphrases do not match."));
            return false;
        }
    }
    return true;
}

bool MnemonicBackupDialog::generateAndShowMnemonic()
{
    const size_t word_count = ui->words24Radio->isChecked() ? 24 : 12;
    try {
        if (!m_mnemonic.empty()) {
            memory_cleanse(m_mnemonic.data(), m_mnemonic.size());
            m_mnemonic.clear();
        }
        m_mnemonic = Mnemonic::Generate(word_count);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Mnemonic generation failed"), QString::fromStdString(e.what()));
        return false;
    }

    std::string error;
    if (!Mnemonic::Validate(m_mnemonic, error)) {
        QMessageBox::critical(this, tr("Mnemonic generation failed"),
                              tr("Generated phrase failed validation: %1").arg(QString::fromStdString(error)));
        return false;
    }

    ui->wroteDownCheckBox->setChecked(false);
    ui->showMnemonicCheckBox->setChecked(true);
    m_mnemonic_visible = true;
    refreshMnemonicDisplay();
    return true;
}

void MnemonicBackupDialog::refreshMnemonicDisplay()
{
    if (m_mnemonic.empty()) {
        ui->mnemonicDisplay->clear();
        return;
    }
    const QString phrase = QString::fromUtf8(m_mnemonic.data(), static_cast<int>(m_mnemonic.size()));
    if (!m_mnemonic_visible) {
        const QStringList words = phrase.split(QRegularExpression(QStringLiteral("\\s+")), QString::SkipEmptyParts);
        QStringList masked;
        masked.reserve(words.size());
        for (int i = 0; i < words.size(); ++i) {
            masked.append(QStringLiteral("%1. ••••••").arg(i + 1));
        }
        ui->mnemonicDisplay->setPlainText(masked.join(QLatin1Char('\n')));
        return;
    }

    const QStringList words = phrase.split(QRegularExpression(QStringLiteral("\\s+")), QString::SkipEmptyParts);
    QStringList numbered;
    numbered.reserve(words.size());
    for (int i = 0; i < words.size(); ++i) {
        numbered.append(QStringLiteral("%1. %2").arg(i + 1).arg(words.at(i)));
    }
    ui->mnemonicDisplay->setPlainText(numbered.join(QLatin1Char('\n')));
}

QString MnemonicBackupDialog::normalizeMnemonic(const QString& text)
{
    const QStringList words = text.trimmed().toLower().split(
        QRegularExpression(QStringLiteral("\\s+")), QString::SkipEmptyParts);
    return words.join(QLatin1Char(' '));
}

bool MnemonicBackupDialog::validateConfirm()
{
    const QString expected = normalizeMnemonic(
        QString::fromUtf8(m_mnemonic.data(), static_cast<int>(m_mnemonic.size())));
    const QString entered = normalizeMnemonic(ui->confirmMnemonicEdit->toPlainText());
    if (entered != expected) {
        ui->confirmStatusLabel->setStyleSheet(QStringLiteral("color: #c62828; font-weight: 600;"));
        ui->confirmStatusLabel->setText(tr("Words do not match. Check spelling and order, then try again."));
        return false;
    }
    ui->confirmStatusLabel->setStyleSheet(QStringLiteral("color: #2e7d32; font-weight: 600;"));
    ui->confirmStatusLabel->setText(tr("Mnemonic confirmed."));
    return true;
}

void MnemonicBackupDialog::onEncryptToggled(bool enabled)
{
    ui->passphraseLabel->setEnabled(enabled);
    ui->confirmLabel->setEnabled(enabled);
    ui->passphraseEdit->setEnabled(enabled);
    ui->confirmEdit->setEnabled(enabled);
    ui->showWalletPassCheckBox->setEnabled(enabled);
    if (!enabled) {
        ui->passphraseEdit->clear();
        ui->confirmEdit->clear();
    }
    updateSetupNextEnabled();
}

void MnemonicBackupDialog::onBip44Toggled(bool enabled)
{
    ui->legacyCoinTypeCheckBox->setEnabled(enabled);
    if (!enabled) {
        ui->legacyCoinTypeCheckBox->setChecked(false);
    }
}

void MnemonicBackupDialog::onShowWalletPassToggled(bool show)
{
    const auto mode = show ? QLineEdit::Normal : QLineEdit::Password;
    ui->passphraseEdit->setEchoMode(mode);
    ui->confirmEdit->setEchoMode(mode);
}

void MnemonicBackupDialog::onShowBip39PassToggled(bool show)
{
    ui->bip39PassEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

void MnemonicBackupDialog::onShowMnemonicToggled(bool show)
{
    m_mnemonic_visible = show;
    refreshMnemonicDisplay();
}

void MnemonicBackupDialog::onWroteDownToggled(bool checked)
{
    if (ui->stackedWidget->currentIndex() == PageShow) {
        ui->nextButton->setEnabled(checked);
    }
}

void MnemonicBackupDialog::updateSetupNextEnabled()
{
    if (ui->stackedWidget->currentIndex() != PageSetup) {
        return;
    }
    bool ok = !walletName().isEmpty();
    if (encryptWallet()) {
        ok = ok && !ui->passphraseEdit->text().isEmpty() && !ui->confirmEdit->text().isEmpty();
    }
    ui->nextButton->setEnabled(ok);
}

void MnemonicBackupDialog::updateConfirmNextEnabled()
{
    if (ui->stackedWidget->currentIndex() != PageConfirm) {
        return;
    }
    ui->nextButton->setEnabled(!ui->confirmMnemonicEdit->toPlainText().trimmed().isEmpty());
}
