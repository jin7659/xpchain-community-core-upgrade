// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/stakingrewardsettingmodel.h>

#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <key_io.h>
#include <wallet/wallet.h>

#include <QFont>
#include <QDebug>

struct StakingRewardSettingTableEntry
{
    QString label;
    QString address;
    quint8 distributionPercentage;

    StakingRewardSettingTableEntry() {}
    StakingRewardSettingTableEntry(const QString &_label, const QString &_address, const quint8 _distributionPercentage):
        label(_label), address(_address), distributionPercentage(_distributionPercentage) {}
};

struct StakingRewardSettingTableEntryLessThan
{
    bool operator()(const StakingRewardSettingTableEntry &a, const StakingRewardSettingTableEntry &b) const
    {
        return a.address < b.address;
    }
    bool operator()(const StakingRewardSettingTableEntry &a, const QString &b) const
    {
        return a.address < b;
    }
    bool operator()(const QString &a, const StakingRewardSettingTableEntry &b) const
    {
        return a < b.address;
    }
};

// Private implementation
class StakingRewardSettingTablePriv
{
public:
    QList<StakingRewardSettingTableEntry> settingEntriesTable;
    StakingRewardSettingTableModel *parent;

    StakingRewardSettingTablePriv(StakingRewardSettingTableModel *_parent):
        parent(_parent) {}

    void refreshAddressTable(interfaces::Wallet& wallet)
    {
        settingEntriesTable.clear();
        {
            for (const auto& address : wallet.getRewardDistributionPcts())
            {
                // Get label of address from wallet's address book
                std::string label;
                wallet.getAddress(DecodeDestination(address.first) , &label, /* is_mine= */ nullptr, /* purpose= */ nullptr);

                settingEntriesTable.append(StakingRewardSettingTableEntry(
                    QString::fromStdString(label),
                    QString::fromStdString(address.first),
                    address.second
                ));
            }
        }
        // qLowerBound() and qUpperBound() require our settingEntriesTable list to be sorted in asc order
        // Even though the map is already sorted this re-sorting step is needed because the originating map
        // is sorted by binary address, not by base58() address.
        qSort(settingEntriesTable.begin(), settingEntriesTable.end(), StakingRewardSettingTableEntryLessThan());
    }

    /* Reusable function of getting iterators and indexes of entry corresponding with address*/
    void findEntry(const QString &address,
        QList<StakingRewardSettingTableEntry>::iterator* lower,
        QList<StakingRewardSettingTableEntry>::iterator* upper,
        int* lowerIndex,
        int* upperIndex)
    {
        QList<StakingRewardSettingTableEntry>::iterator _lower = qLowerBound(
                settingEntriesTable.begin(), settingEntriesTable.end(), address, StakingRewardSettingTableEntryLessThan());
        QList<StakingRewardSettingTableEntry>::iterator _upper = qUpperBound(
                settingEntriesTable.begin(), settingEntriesTable.end(), address, StakingRewardSettingTableEntryLessThan());
        if(lower)
            *lower = _lower;
        if(upper)
            *upper = _upper;
        if(lowerIndex)
            *lowerIndex = (_lower - settingEntriesTable.begin());
        if(upperIndex)
            *upperIndex = (_upper - settingEntriesTable.begin());
    }

    void updateEntry(const QString &address, const QString &label, int status)
    {
        // Find address / label in model
        QList<StakingRewardSettingTableEntry>::iterator lower, upper;
        int lowerIndex;
        findEntry(address, &lower, &upper, &lowerIndex, /* upperIndex= */ nullptr);

        bool inModel = (lower != upper);

        if(status == CT_UPDATED)
        {
            if(!inModel)
            {
                qWarning() << "StakingRewardSettingTablePriv::updateEntry: Warning: Got CT_UPDATED, but entry is not in model";
                return;
            }
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            return;
        } else if(status == CT_NEW && inModel)
        {
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            return;
        }
    }

    int size()
    {
        return settingEntriesTable.size();
    }

    StakingRewardSettingTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < settingEntriesTable.size())
        {
            return &settingEntriesTable[idx];
        }
        else
        {
            return 0;
        }
    }
};

StakingRewardSettingTableModel::StakingRewardSettingTableModel(WalletModel *parent) :
    QAbstractTableModel(parent), walletModel(parent)
{
    columns << tr("Label") << tr("Address") <<  tr("Distribution %");
    priv = new StakingRewardSettingTablePriv(this);
    priv->refreshAddressTable(parent->wallet());
}

StakingRewardSettingTableModel::~StakingRewardSettingTableModel()
{
    delete priv;
}

int StakingRewardSettingTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int StakingRewardSettingTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant StakingRewardSettingTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    StakingRewardSettingTableEntry *rec = static_cast<StakingRewardSettingTableEntry*>(index.internalPointer());

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Label:
            if(rec->label.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->label;
            }
        case Address:
            return rec->address;
        case DistributionPercentage:
            // Return string of it (e.g. "35 %") if role == Qt::DisplayRole
            // Otherwise, return original number (e.g. 35)
            // Wrap these values with QVariant because a tertiary operator only can return two values of the same type
            return role == Qt::DisplayRole ? QVariant(QString::number(rec->distributionPercentage) + " %")  : QVariant(rec->distributionPercentage);
        }
    }
    else if (role == Qt::FontRole)
    {
        QFont font;
        if(index.column() == Address)
        {
            font = GUIUtil::fixedPitchFont();
        }
        return font;
    }
    else if (role == Qt::TextAlignmentRole)
    {
        if(index.column() == DistributionPercentage)
        {
            int align = Qt::AlignRight | Qt::AlignVCenter;
            return align;
        }
    }
    return QVariant();
}

bool StakingRewardSettingTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid())
        return false;
    StakingRewardSettingTableEntry *rec = static_cast<StakingRewardSettingTableEntry*>(index.internalPointer());
    editStatus = OK;
    if(role == Qt::EditRole)
    {
        CTxDestination curAddress = DecodeDestination(rec->address.toStdString());
        if(index.column() == Label)
        {
            // Do nothing, if old label == new label
            if(rec->label == value.toString())
            {
                editStatus = NO_CHANGES;
                return false;
            }
            walletModel->wallet().setAddressBook(curAddress, value.toString().toStdString(), "");
        } else if(index.column() == Address) {
            QString newQStrAddress = value.toString();
            CTxDestination newAddress = DecodeDestination(newQStrAddress.toStdString());
            // Refuse to set invalid address, set error status and return false
            if(boost::get<CNoDestination>(&newAddress))
            {
                editStatus = INVALID_ADDRESS;
                return false;
            }
            // Do nothing, if old address == new address
            if(newAddress == curAddress)
            {
                editStatus = NO_CHANGES;
                return false;
            }
            // Check for duplicate addresses to prevent accidental deletion of addresses, if you try
            // to paste an existing address over another address (with a different label)
            if (lookupAddress(value.toString()) != -1)
            {
                editStatus = DUPLICATE_ADDRESS;
                return false;
            }
            // Directly re-assign a new address to the entry's member
            rec->address = value.toString();
            emitDataChanged(index.row());
        } else if(index.column() == DistributionPercentage) {
            // Check whether value is quint8
            if(!value.canConvert<quint8>())
            {
                editStatus = INVALID_PERCENTAGE;
                return false;
            }
            // Check whether value is between 1 - 100
            quint8 pct = value.value<quint8>();
            if(pct == 0 || pct > 100)
            {
                editStatus = INVALID_PERCENTAGE;
                return false;
            }
            rec->distributionPercentage = pct;
            emitDataChanged(index.row());
        }
        return true;
    }
    return false;
}

QVariant StakingRewardSettingTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags StakingRewardSettingTableModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;
    StakingRewardSettingTableEntry *rec = static_cast<StakingRewardSettingTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    return retval;
}

QModelIndex StakingRewardSettingTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    StakingRewardSettingTableEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void StakingRewardSettingTableModel::updateEntry(const QString &address, const QString &label, int status)
{
    // Update address book model from XPChain core
    priv->updateEntry(address, label, status);
}

QString StakingRewardSettingTableModel::addRow(const QString &label, const QString &address, const quint8 percentage , const OutputType address_type)
{
    // std::string strLabel = label.toStdString();
    QString _label = label;
    std::string strAddress = address.toStdString();

    editStatus = OK;
    // Refuse to set invalid address, set error status and return false
    if(!walletModel->validateAddress(address))
    {
        editStatus = INVALID_ADDRESS;
        return QString();
    }
    // Check for duplicate addresses
    if (lookupAddress(address) != -1)
    {
        editStatus = DUPLICATE_ADDRESS;
        return QString();
    }
    // Check whether the percentage is from 1 to 100
    if(percentage == 0 || percentage > 100)
    {
        editStatus = INVALID_PERCENTAGE;
        return QString();
    }

    if(_label.isEmpty())
    {
        _label = labelForAddress(address);
    }

    // Add entry
    int lowerIndex;
    priv->findEntry(address, /* lower= */ nullptr, /* upper= */ nullptr, &lowerIndex, /* upperIndex= */ nullptr);

    beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
    priv->settingEntriesTable.insert(lowerIndex, StakingRewardSettingTableEntry(_label, address, percentage));
    endInsertRows();

    return QString::fromStdString(strAddress);
}

bool StakingRewardSettingTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    StakingRewardSettingTableEntry *rec = priv->index(row);


    QList<StakingRewardSettingTableEntry>::iterator lower, upper;
    int lowerIndex, upperIndex;
    priv->findEntry(rec->address, &lower, &upper, &lowerIndex, &upperIndex);
    bool inModel = (lower != upper);
    if(count != 1 || !inModel)
    {
        // Can only remove one row at a time, and cannot remove rows not in model.
        return false;
    }
    beginRemoveRows(QModelIndex(), lowerIndex, upperIndex - 1);
    priv->settingEntriesTable.erase(lower, upper);
    endRemoveRows();
    return true;
}

QString StakingRewardSettingTableModel::labelForAddress(const QString &address) const
{
    std::string name;
    if (getAddressData(address, &name, /* purpose= */ nullptr)) {
        return QString::fromStdString(name);
    }
    return QString();
}

QString StakingRewardSettingTableModel::purposeForAddress(const QString &address) const
{
    std::string purpose;
    if (getAddressData(address, /* name= */ nullptr, &purpose)) {
        return QString::fromStdString(purpose);
    }
    return QString();
}

bool StakingRewardSettingTableModel::getAddressData(const QString &address,
        std::string* name,
        std::string* purpose) const {
    CTxDestination destination = DecodeDestination(address.toStdString());
    return walletModel->wallet().getAddress(destination, name, /* is_mine= */ nullptr, purpose);
}

int StakingRewardSettingTableModel::lookupAddress(const QString &address) const
{
    QModelIndexList lst = match(index(0, Address, QModelIndex()),
                                Qt::EditRole, address, 1, Qt::MatchExactly);
    if(lst.isEmpty())
    {
        return -1;
    }
    else
    {
        return lst.at(0).row();
    }
}

OutputType StakingRewardSettingTableModel::GetDefaultAddressType() const { return walletModel->wallet().getDefaultAddressType(); };

void StakingRewardSettingTableModel::emitDataChanged(int idx)
{
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length()-1, QModelIndex()));
}
