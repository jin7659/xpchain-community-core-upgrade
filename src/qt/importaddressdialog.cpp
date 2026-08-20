// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/importaddressdialog.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

ImportAddressDialog::ImportAddressDialog(QWidget* parent)
    : QDialog(parent),
      m_addressEdit(nullptr),
      m_labelEdit(nullptr),
      m_rescanCheck(nullptr)
{
    setWindowTitle(tr("Import Watch-Only Address"));
    setMinimumWidth(440);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* tip = new QLabel(
        tr("Adds an address you do not have keys for. Balances and history appear after a rescan. "
           "You cannot spend from watch-only addresses."), this);
    tip->setWordWrap(true);
    layout->addWidget(tip);

    QFormLayout* form = new QFormLayout();
    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setPlaceholderText(tr("Address"));
    form->addRow(tr("Address"), m_addressEdit);

    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setPlaceholderText(tr("Optional label"));
    form->addRow(tr("Label"), m_labelEdit);

    m_rescanCheck = new QCheckBox(tr("Rescan blockchain for transactions (may take a while)"), this);
    m_rescanCheck->setChecked(true);
    form->addRow(QString(), m_rescanCheck);

    layout->addLayout(form);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString ImportAddressDialog::address() const
{
    return m_addressEdit ? m_addressEdit->text().trimmed() : QString();
}

QString ImportAddressDialog::label() const
{
    return m_labelEdit ? m_labelEdit->text().trimmed() : QString();
}

bool ImportAddressDialog::rescan() const
{
    return m_rescanCheck && m_rescanCheck->isChecked();
}
