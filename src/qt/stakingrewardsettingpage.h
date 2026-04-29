// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_STAKINGREWARDSETTINGPAGE_H
#define XPCHAIN_QT_STAKINGREWARDSETTINGPAGE_H

#include <QDialog>

class StakingRewardSettingTableSortFilterProxyModel;
class StakingRewardSettingTableModel;
class StakingRewardSettingEntryDelegate;
class PlatformStyle;

namespace Ui {
    class StakingRewardSettingPage;
}

QT_BEGIN_NAMESPACE
class QItemSelection;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

/** Widget that shows a list of sending or receiving addresses.
  */
class StakingRewardSettingPage : public QDialog
{
    Q_OBJECT

public:

    explicit StakingRewardSettingPage(const PlatformStyle *platformStyle, /*Mode mode, Tabs tab,*/ QWidget *parent = 0);
    ~StakingRewardSettingPage();

    void setModel(StakingRewardSettingTableModel *model);
    const std::vector<std::pair<std::string, std::uint8_t>> &getReturnValue() const { return returnValue; }

public Q_SLOTS:
    /** Calculate the rest of distribution percentage, and show it (or error messages) to users by labelSurplus */
    void calculateSurplus();

private:
    Ui::StakingRewardSettingPage *ui;
    StakingRewardSettingTableModel *model;
    StakingRewardSettingEntryDelegate *delegate;
    std::vector<std::pair<std::string, std::uint8_t>> returnValue;
    StakingRewardSettingTableSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;
    QAction *deleteAction; // to be able to explicitly disable it
    QString newAddressToSelect;

private Q_SLOTS:
    /** Delete currently selected address entry */
    void on_deleteAddress_clicked();
    /** Create a new address for receiving coins and / or add a new address book entry */
    void on_newAddress_clicked();
    /** Copy address of currently selected address entry to clipboard */
    void on_copyAddress_clicked();
    /** Copy label of currently selected address entry to clipboard (no button) */
    void onCopyLabelAction();
    /** Validate changes and apply them if they are valid */
    void on_okButton_clicked();
    /** Discard changes and close dialog */
    void on_cancelButton_clicked();
    /** Set button states based on selected tab and selection */
    void selectionChanged();
    /** Spawn contextual menu (right mouse menu) for address book entry */
    void contextualMenu(const QPoint &point);
    /** New entry/entries were added to address table */
    void selectNewAddress(const QModelIndex &parent, int begin, int /*end*/);

Q_SIGNALS:
    void sendCoins(QString addr);
};

#endif // XPCHAIN_QT_SRAKINGREWARDSETTINGPAGE_H
