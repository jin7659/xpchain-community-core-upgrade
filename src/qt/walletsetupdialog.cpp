// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletsetupdialog.h>

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

WalletSetupDialog::WalletSetupDialog(QWidget* parent)
    : QDialog(parent),
      m_choice(CreateEmpty),
      m_btnCreate(nullptr),
      m_btnGenerate(nullptr),
      m_btnRestore(nullptr),
      m_btnOpen(nullptr)
{
    setWindowTitle(tr("Set Up Wallet"));
    setMinimumWidth(420);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* intro = new QLabel(
        tr("Choose how to set up this wallet. You can reopen this dialog from File → Set Up Wallet, "
           "or use File → Advanced for individual create / open / mnemonic actions."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QButtonGroup* group = new QButtonGroup(this);

    auto addChoice = [&](const QString& title, const QString& tip) {
        QPushButton* btn = new QPushButton(title, this);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setToolTip(tip);
        btn->setStyleSheet(
            "QPushButton { text-align: left; padding: 10px 12px; }"
            "QPushButton:checked { font-weight: 600; }");
        btn->setMinimumHeight(52);
        layout->addWidget(btn);
        group->addButton(btn);
        return btn;
    };

    m_btnCreate = addChoice(
        tr("Create empty wallet\nModern descriptor wallet (Taproot-ready). Uncheck Descriptor only for BIP39 restore targets."),
        tr("New wallet without a mnemonic. Use for watch-only, advanced import, or as a clean BIP39 restore target (leave Descriptor unchecked)."));
    m_btnGenerate = addChoice(
        tr("Generate & backup mnemonic\nBIP39 seed compatible with XPChain web / mobile recovery."),
        tr("Create a new BIP39 seed, confirm the backup, then create a seeded wallet."));
    m_btnRestore = addChoice(
        tr("Restore from mnemonic\nImport an existing BIP39 phrase into a legacy HD wallet."),
        tr("Import keys from an existing BIP39 mnemonic into a wallet."));
    m_btnOpen = addChoice(
        tr("Open existing wallet file\nLoad a wallet already present in the wallets directory."),
        tr("Load a wallet that already exists on disk."));

    m_btnGenerate->setChecked(true);
    m_choice = GenerateMnemonic;

    connect(group, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(onChoiceClicked(QAbstractButton*)));

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void WalletSetupDialog::onChoiceClicked(QAbstractButton* button)
{
    if (button == m_btnCreate) {
        m_choice = CreateEmpty;
    } else if (button == m_btnGenerate) {
        m_choice = GenerateMnemonic;
    } else if (button == m_btnRestore) {
        m_choice = RestoreMnemonic;
    } else if (button == m_btnOpen) {
        m_choice = OpenExisting;
    }
}
