// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_EDITSTAKINGREWARDDISTRIBUTIONDIALOG_H
#define XPCHAIN_QT_EDITSTAKINGREWARDDISTRIBUTIONDIALOG_H

#include <QDialog>

class StakingRewardSettingTableModel;

namespace Ui {
    class EditStakingRewardDistributionDialog;
}

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

/** Dialog for editing an address and associated information.
 */
class EditStakingRewardDistributionDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        New,
        Edit
    };

    explicit EditStakingRewardDistributionDialog(Mode mode, QWidget *parent = 0);
    ~EditStakingRewardDistributionDialog();

    void setModel(StakingRewardSettingTableModel *model);
    void loadRow(int row);

    QString getAddress() const;
    void setAddress(const QString &address);

public Q_SLOTS:
    void accept();

private:
    bool saveCurrentRow();

    /** Return a descriptive string when adding an already-existing address fails. */
    QString getDuplicateAddressWarning() const;

    Ui::EditStakingRewardDistributionDialog *ui;
    QDataWidgetMapper *mapper;
    Mode mode;
    StakingRewardSettingTableModel *model;

    QString address;
};

#endif // XPCHAIN_QT_EDITSTAKINGREWARDDISTRIBUTIONDIALOG_H
