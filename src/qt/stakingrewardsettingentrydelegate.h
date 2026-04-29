// Copyright (c) 018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_STAKINGREWARDSETTINGENTRYDELEGATE_H
#define XPCHAIN_QT_STAKINGREWARDSETTINGENTRYDELEGATE_H

#include <QStyledItemDelegate>

class StakingRewardSettingEntryDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    StakingRewardSettingEntryDelegate(QWidget *parent = 0) : QStyledItemDelegate(parent) {}

    /* Creates editor when users start to edit */
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    /* Gets current value from model and sets it to editor */
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    /* Gets value from editor and sets it to model */
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private Q_SLOTS:
    /* Double checks for entered value of percentage */
    void commitAndCloseSpinBox();
};

#endif // XPCHAIN_QT_STAKINGREWARDSETTINGENTRYDELEGATE_H
