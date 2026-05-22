// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactiondescdialog.h>
#include <qt/forms/ui_transactiondescdialog.h>

#include <qt/transactiontablemodel.h>
#include <qt/txanalytics.h>

#include <QModelIndex>
#include <QClipboard>
#include <QTimer>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

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

    // SQLite 태그 기능 추가
    QHBoxLayout *tagLayout = new QHBoxLayout();
    QLabel *tagLabel = new QLabel(tr("Tag/Memo:"), this);
    tagLabel->setStyleSheet("font-weight: bold; color: #106ba3;");
    QLineEdit *tagEdit = new QLineEdit(this);
    tagEdit->setPlaceholderText(tr("Enter custom tag or memo for this transaction..."));
    tagEdit->setStyleSheet("padding: 4px; border: 1px solid #106ba3; border-radius: 4px; background-color: #2b2b2b; color: #ffffff;");
    
    // 현재 등록된 태그 로드
    QString currentTag = TxAnalytics::getInstance().getTag(txid);
    tagEdit->setText(currentTag);
    
    QPushButton *saveTagButton = new QPushButton(tr("Save Tag"), this);
    saveTagButton->setStyleSheet("background-color: #106ba3; color: white; border-radius: 4px; padding: 4px 12px; font-weight: bold;");
    
    tagLayout->addWidget(tagLabel);
    tagLayout->addWidget(tagEdit);
    tagLayout->addWidget(saveTagButton);
    
    // verticalLayout의 horizontalLayout 위에 삽입
    ui->verticalLayout->insertLayout(ui->verticalLayout->count() - 1, tagLayout);
    
    connect(saveTagButton, &QPushButton::clicked, this, [txid, tagEdit, saveTagButton]() {
        QString tagText = tagEdit->text().trimmed();
        if (TxAnalytics::getInstance().setTag(txid, tagText)) {
            saveTagButton->setText(tr("Saved!"));
            saveTagButton->setStyleSheet("background-color: #2ecc71; color: white; border-radius: 4px; padding: 4px 12px; font-weight: bold;");
            QTimer::singleShot(1000, saveTagButton, [saveTagButton]() {
                saveTagButton->setText(tr("Save Tag"));
                saveTagButton->setStyleSheet("background-color: #106ba3; color: white; border-radius: 4px; padding: 4px 12px; font-weight: bold;");
            });
        } else {
            saveTagButton->setText(tr("Error!"));
            saveTagButton->setStyleSheet("background-color: #e74c3c; color: white; border-radius: 4px; padding: 4px 12px; font-weight: bold;");
            QTimer::singleShot(1000, saveTagButton, [saveTagButton]() {
                saveTagButton->setText(tr("Save Tag"));
                saveTagButton->setStyleSheet("background-color: #106ba3; color: white; border-radius: 4px; padding: 4px 12px; font-weight: bold;");
            });
        }
    });
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
