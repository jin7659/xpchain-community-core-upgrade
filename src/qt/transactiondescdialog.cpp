// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactiondescdialog.h>
#include <qt/forms/ui_transactiondescdialog.h>

#include <qt/transactiontablemodel.h>

#include <QModelIndex>
#include <QClipboard>
#include <QTimer>
#include <QApplication>

TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Details for %1").arg(idx.data(TransactionTableModel::TxHashRole).toString()));
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);

    QString txid = idx.data(TransactionTableModel::TxHashRole).toString();
    connect(ui->copyTxIDButton, &QPushButton::clicked, this, [this, txid]() {
        QApplication::clipboard()->setText(txid);
        ui->copyTxIDButton->setText(tr("Copied!"));
        QTimer::singleShot(1000, this, [this]() {
            ui->copyTxIDButton->setText(tr("Copy TXID"));
        });
    });
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
