// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/xpchain-config.h>
#endif

#include <qt/stakingrewardsettingpage.h>
#include <qt/forms/ui_stakingrewardsettingpage.h>
#include <qt/editstakingrewarddistributiondialog.h>
#include <qt/stakingrewardsettingmodel.h>
#include <qt/stakingrewardsettingentrydelegate.h>
#include <qt/xpchaingui.h>
#include <qt/csvmodelwriter.h>
#include <qt/guiutil.h>
#include <qt/platformstyle.h>

#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>

class StakingRewardSettingTableSortFilterProxyModel final : public QSortFilterProxyModel
{
    const QString m_type;

public:
    StakingRewardSettingTableSortFilterProxyModel(QObject* parent)
        : QSortFilterProxyModel(parent)
    {
        setDynamicSortFilter(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setSortCaseSensitivity(Qt::CaseInsensitive);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const
    {
        auto model = sourceModel();
        auto label = model->index(row, StakingRewardSettingTableModel::Label, parent);
        auto address = model->index(row, StakingRewardSettingTableModel::Address, parent);

        if (filterRegExp().indexIn(model->data(address).toString()) < 0 &&
            filterRegExp().indexIn(model->data(label).toString()) < 0) {
            return false;
        }

        return true;
    }
};

StakingRewardSettingPage::StakingRewardSettingPage(const PlatformStyle *platformStyle, /*Mode _mode, Tabs _tab,*/ QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StakingRewardSettingPage),
    model(0)
{
    ui->setupUi(this);

    if (!platformStyle->getImagesOnButtons()) {
        ui->newAddress->setIcon(QIcon());
        ui->copyAddress->setIcon(QIcon());
        ui->deleteAddress->setIcon(QIcon());
    } else {
        ui->newAddress->setIcon(platformStyle->SingleColorIcon(":/icons/add"));
        ui->copyAddress->setIcon(platformStyle->SingleColorIcon(":/icons/editcopy"));
        ui->deleteAddress->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    }

    // Context menu actions
    QAction *copyAddressAction = new QAction(tr("&Copy Address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    deleteAction = new QAction(ui->deleteAddress->text(), this);

    // Build context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();

    // Connect signals for context menu actions
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyAddress_clicked()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(onCopyLabelAction()));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(on_deleteAddress_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
}

StakingRewardSettingPage::~StakingRewardSettingPage()
{
    delete ui;
}

void StakingRewardSettingPage::setModel(StakingRewardSettingTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;

    proxyModel = new StakingRewardSettingTableSortFilterProxyModel(/*type, */ this);
    proxyModel->setSourceModel(_model);

    connect(ui->searchLineEdit, SIGNAL(textChanged(QString)), proxyModel, SLOT(setFilterWildcard(QString)));

    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    // Set column widths
    ui->tableView->horizontalHeader()->setSectionResizeMode(StakingRewardSettingTableModel::Label, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(StakingRewardSettingTableModel::Address, QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(StakingRewardSettingTableModel::DistributionPercentage, QHeaderView::ResizeToContents);

    // Install delegate between model and view
    delegate = new StakingRewardSettingEntryDelegate(this);
    ui->tableView->setItemDelegate(delegate);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
        this, SLOT(selectionChanged()));

    // Select row for newly created address
    connect(_model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(selectNewAddress(QModelIndex,int,int)));

    // Update text of labelSuplus for displaying the number of percentage remaining or some errors
    connect(_model, SIGNAL(dataChanged(QModelIndex, QModelIndex, QVector<int>)), this, SLOT(calculateSurplus()));
    connect(_model, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(calculateSurplus()));
    connect(_model, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(calculateSurplus()));

    selectionChanged();
    calculateSurplus();
}

void StakingRewardSettingPage::calculateSurplus()
{
    QAbstractItemModel *model = ui->tableView->model();
    uint8_t totalPcts = 0;

    for (int r = 0; r < model->rowCount(); ++r)
    {
        totalPcts += model->data(model->index(r, StakingRewardSettingTableModel::ColumnIndex::DistributionPercentage), Qt::EditRole).toInt();
    }

    if(totalPcts == 0) // All of a reward will be sent to the capital address.
    {
        ui->labelSurplus->setText(tr("An entire reward will be sent to the captal address."));
        ui->labelSurplus->setStyleSheet(QString());
        ui->okButton->setEnabled(true);
    } else if(totalPcts < 100) // The rest will be sent back to the capital address.
    {
        ui->labelSurplus->setText(tr("About %1% remaining of a reward will be sent back to the captal address.").arg(100 - totalPcts));
        ui->labelSurplus->setStyleSheet(QString());
        ui->okButton->setEnabled(true);
    } else if(totalPcts == 100) // No coins will be sent back.
    {
        ui->labelSurplus->setText(
            tr("Almost all reward will be sent as above."));
        ui->labelSurplus->setStyleSheet(QString());
        ui->okButton->setEnabled(true);
    } else // Error: thte total of percentage exceeds maximum
    {
        ui->labelSurplus->setText(tr("The total of distribution percentage must be 100% or less."));
        ui->labelSurplus->setStyleSheet("color: red;");
        ui->okButton->setEnabled(false);
    }
}

void StakingRewardSettingPage::on_copyAddress_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, StakingRewardSettingTableModel::Address);
}

void StakingRewardSettingPage::onCopyLabelAction()
{
    GUIUtil::copyEntryData(ui->tableView, StakingRewardSettingTableModel::Label);
}

void StakingRewardSettingPage::on_newAddress_clicked()
{
    if(!model)
        return;

    EditStakingRewardDistributionDialog dlg(EditStakingRewardDistributionDialog::New, this);
    dlg.setModel(model);

    if(dlg.exec())
    {
        newAddressToSelect = dlg.getAddress();
        // Inserting rows doesn't resize columns automatically, so we must call this manually
        ui->tableView->resizeColumnToContents(StakingRewardSettingTableModel::Address);
    }
}

void StakingRewardSettingPage::on_deleteAddress_clicked()
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    QModelIndexList indexes = table->selectionModel()->selectedRows();
    if(!indexes.isEmpty())
    {
        table->model()->removeRow(indexes.at(0).row());
    }
}

void StakingRewardSettingPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        ui->deleteAddress->setEnabled(true);
        ui->deleteAddress->setVisible(true);
        deleteAction->setEnabled(true);
        ui->copyAddress->setEnabled(true);
    }
    else
    {
        ui->deleteAddress->setEnabled(false);
        ui->copyAddress->setEnabled(false);
    }
}

void StakingRewardSettingPage::on_okButton_clicked()
{
    QTableView *table = ui->tableView;
    if(!table->model())
        return;

    // Calculate the total of percentages
    QAbstractItemModel *model = table->model();
    std::vector<std::pair<std::string, std::uint8_t>> vPcts;
    uint8_t totalPct = 0;

    for (int r = 0; r < model->rowCount(); ++r)
    {
        std::string strAddress =
            model->data(model->index(r, StakingRewardSettingTableModel::ColumnIndex::Address)).toString().toStdString();
        uint8_t pct =
            model->data(model->index(r, StakingRewardSettingTableModel::ColumnIndex::DistributionPercentage), Qt::EditRole).toInt();

        vPcts.push_back(std::make_pair(strAddress, pct));
        totalPct += pct;
    }

    if(totalPct > 100)
    {
        // There's almost no chance to get here because ok button is automatically disabled in calculating of surplus.
        QMessageBox::critical(this, windowTitle(), tr("The total of distribution percentage must be 100% or less."),
            QMessageBox::Ok, QMessageBox::Ok);
        returnValue = std::vector<std::pair<std::string, std::uint8_t>>();
        return;
    }

    returnValue = vPcts;
    accept();
}

void StakingRewardSettingPage::on_cancelButton_clicked()
{
    reject();
}

void StakingRewardSettingPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void StakingRewardSettingPage::selectNewAddress(const QModelIndex &parent, int begin, int /*end*/)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, StakingRewardSettingTableModel::Address, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAddressToSelect))
    {
        // Select row of newly created address, once
        ui->tableView->setFocus();
        ui->tableView->selectRow(idx.row());
        newAddressToSelect.clear();
    }
}
