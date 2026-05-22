#include <qt/watchaddressdialog.h>
#include <qt/forms/ui_watchaddressdialog.h>
#include <qt/txanalytics.h>
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>
#include <key_io.h>


WatchAddressDialog::WatchAddressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WatchAddressDialog)
{
    ui->setupUi(this);

    // 테이블 뷰 설정
    ui->addressTable->setColumnCount(4);
    ui->addressTable->setHorizontalHeaderLabels(QStringList() << tr("Address") << tr("Label") << tr("Balance (XPC)") << tr("Last Updated"));
    ui->addressTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->addressTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->addressTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->addressTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->addressTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->addressTable->setSelectionMode(QAbstractItemView::SingleSelection);

    ui->deleteButton->setEnabled(false);

    refreshList();
}

WatchAddressDialog::~WatchAddressDialog()
{
    delete ui;
}

void WatchAddressDialog::refreshList()
{
    ui->addressTable->setRowCount(0);
    QList<TxAnalytics::WatchAddress> list = TxAnalytics::getInstance().getWatchAddresses();

    for (int i = 0; i < list.size(); ++i) {
        ui->addressTable->insertRow(i);

        QTableWidgetItem *itemAddr = new QTableWidgetItem(list[i].address);
        QTableWidgetItem *itemLabel = new QTableWidgetItem(list[i].label);
        
        // 잔고 포맷팅: 소수점 4자리까지 표시
        QTableWidgetItem *itemBalance = new QTableWidgetItem(QString::number(list[i].balance, 'f', 4));
        itemBalance->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // 업데이트 일시 포맷팅
        QString dateStr = tr("Never");
        if (list[i].updatedAt > 0) {
            dateStr = QDateTime::fromSecsSinceEpoch(list[i].updatedAt).toString("yyyy-MM-dd hh:mm:ss");
        }
        QTableWidgetItem *itemUpdate = new QTableWidgetItem(dateStr);
        itemUpdate->setTextAlignment(Qt::AlignCenter);

        // 읽기 전용으로 설정
        itemAddr->setFlags(itemAddr->flags() & ~Qt::ItemIsEditable);
        itemLabel->setFlags(itemLabel->flags() & ~Qt::ItemIsEditable);
        itemBalance->setFlags(itemBalance->flags() & ~Qt::ItemIsEditable);
        itemUpdate->setFlags(itemUpdate->flags() & ~Qt::ItemIsEditable);

        ui->addressTable->setItem(i, 0, itemAddr);
        ui->addressTable->setItem(i, 1, itemLabel);
        ui->addressTable->setItem(i, 2, itemBalance);
        ui->addressTable->setItem(i, 3, itemUpdate);
    }
}

bool WatchAddressDialog::validateAddress(const QString& address)
{
    QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) return false;
    
    return IsValidDestinationString(trimmed.toStdString());
}

void WatchAddressDialog::on_addButton_clicked()
{
    QString address = ui->addressEdit->text().trimmed();
    QString label = ui->labelEdit->text().trimmed();

    if (!validateAddress(address)) {
        QMessageBox::warning(this, tr("Invalid Address"), tr("Please enter a valid XPChain address."));
        return;
    }

    if (label.isEmpty()) {
        label = tr("Web Wallet");
    }

    // 이미 존재하는지 확인
    QList<TxAnalytics::WatchAddress> list = TxAnalytics::getInstance().getWatchAddresses();
    for (const auto& item : list) {
        if (item.address == address) {
            QMessageBox::warning(this, tr("Duplicate Address"), tr("This address is already registered."));
            return;
        }
    }

    if (TxAnalytics::getInstance().addWatchAddress(address, label)) {
        ui->addressEdit->clear();
        ui->labelEdit->clear();
        refreshList();
    } else {
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to save the watch address to database."));
    }
}

void WatchAddressDialog::on_deleteButton_clicked()
{
    int currentRow = ui->addressTable->currentRow();
    if (currentRow < 0) return;

    QTableWidgetItem *item = ui->addressTable->item(currentRow, 0);
    if (!item) return;

    QString address = item->text();

    if (QMessageBox::question(this, tr("Delete Address"),
                               tr("Are you sure you want to stop watching address %1?").arg(address),
                               QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        if (TxAnalytics::getInstance().removeWatchAddress(address)) {
            refreshList();
        } else {
            QMessageBox::critical(this, tr("Database Error"), tr("Failed to delete the address from database."));
        }
    }
}

void WatchAddressDialog::on_closeButton_clicked()
{
    accept();
}

void WatchAddressDialog::on_addressTable_itemSelectionChanged()
{
    ui->deleteButton->setEnabled(ui->addressTable->currentRow() >= 0);
}
