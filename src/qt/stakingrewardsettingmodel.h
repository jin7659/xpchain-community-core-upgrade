// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_STAKINGREWARDSETTINGMODEL_H
#define XPCHAIN_QT_STAKINGREWARDSETTINGMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

enum class OutputType;

class StakingRewardSettingTablePriv;
class WalletModel;

namespace interfaces {
class Wallet;
}

/**
   Qt model of the address book in the core. This allows views to access and modify the address book.
 */
class StakingRewardSettingTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit StakingRewardSettingTableModel(WalletModel *parent = 0);
    ~StakingRewardSettingTableModel();

    enum ColumnIndex {
        Label = 0,   /**< User specified label */
        Address = 1, /**< XPChain address */
        DistributionPercentage = 2  /**< Reward percentage */
    };

    enum RoleIndex {
        TypeRole = Qt::UserRole /**< Type of address (#Send or #Receive) */
    };

    /** Return status of edit/insert operation */
    enum EditStatus {
        OK,                     /**< Everything ok */
        NO_CHANGES,             /**< No changes were made during edit operation */
        INVALID_ADDRESS,        /**< Unparseable address */
        DUPLICATE_ADDRESS,      /**< Address already in settings */
        INVALID_PERCENTAGE,     /**< Number of the percentage is not uint8, or exceeds 100 alone*/
        EXCEEDED_MAX            /**< The total percentages exceeds 100*/
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex &index) const;
    /*@}*/

    /* Add an address to the model.
       Returns the added address on success, and an empty string otherwise.
     */
    QString addRow(const QString &label, const QString &address, const quint8 percentage, const OutputType address_type);

    /** Look up label for address in address book, if not found return empty string. */
    QString labelForAddress(const QString &address) const;

    /** Look up purpose for address in address book, if not found return empty string. */
    QString purposeForAddress(const QString &address) const;

    /* Look up row index of an address in the model.
       Return -1 if not found.
     */
    int lookupAddress(const QString &address) const;

    /* Returns the most recent result of editing.
     */
    EditStatus getEditStatus() const { return editStatus; }

    OutputType GetDefaultAddressType() const;

private:
    WalletModel* const walletModel;
    StakingRewardSettingTablePriv *priv = nullptr;
    QStringList columns;
    EditStatus editStatus = OK;

    /** Look up address book data given an address string. */
    bool getAddressData(const QString &address, std::string* name, std::string* purpose) const;

    /** Notify listeners that data changed. */
    void emitDataChanged(int index);

public Q_SLOTS:
    /* Update address list from core.
     */
    void updateEntry(const QString &address, const QString &label, int status);

    friend class StakingRewardSettingTablePriv;
};

#endif // XPCHAIN_QT_STAKINGREWARDSETTINGMODEL_H
