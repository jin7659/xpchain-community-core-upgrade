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
        // Match wallet accent (#1f6feb / #6cb6ff) so the selected choice is obvious.
        btn->setStyleSheet(
            "QPushButton {"
            "  text-align: left;"
            "  padding: 10px 12px;"
            "  border: 1px solid #3d444d;"
            "  border-radius: 4px;"
            "  background-color: transparent;"
            "}"
            "QPushButton:hover {"
            "  border-color: #1f6feb;"
            "  background-color: rgba(31, 111, 235, 40);"
            "}"
            "QPushButton:checked {"
            "  font-weight: 600;"
            "  color: #6cb6ff;"
            "  border: 1px solid #1f6feb;"
            "  background-color: rgba(31, 111, 235, 70);"
            "}");
        btn->setMinimumHeight(52);
        layout->addWidget(btn);
        group->addButton(btn);
        return btn;
    };

    m_btnCreate = addChoice(
        tr("Create empty wallet\nModern descriptor wallet (Taproot-ready). Uncheck Descriptor only for BIP39 restore targets."),
        tr("New wallet without a mnemonic. Use for watch-only, advanced import, or as a clean BIP39 restore target (leave Descriptor unchecked)."));
    m_btnGenerate = addChoice(
        tr("Create wallet with new mnemonic\nGenerate a BIP39 seed, back it up, then create a new seeded wallet."),
        tr("Starts a new wallet: generate a BIP39 seed, confirm the backup, then create a wallet from that seed. Does not attach a mnemonic to an existing wallet."));
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
