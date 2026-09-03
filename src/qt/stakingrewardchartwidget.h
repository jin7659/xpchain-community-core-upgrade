// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_STAKINGREWARDCHARTWIDGET_H
#define XPCHAIN_QT_STAKINGREWARDCHARTWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QString>
#include <QRect>

class WalletModel;

struct MonthlyStakingStats {
    QString monthKey;      // e.g. "2026-08"
    QString monthLabel;    // e.g. "8월" or "Aug"
    qint64 totalAmount;    // in Satoshis
    int stakeCount;        // number of PoS mint transactions
};

class StakingRewardChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StakingRewardChartWidget(QWidget *parent = nullptr);
    ~StakingRewardChartWidget() override;

    void setWalletModel(WalletModel *model);

public Q_SLOTS:
    void updateData();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    WalletModel *walletModel;
    QVector<MonthlyStakingStats> m_monthlyData;
    QVector<QRect> m_barRects;
    int m_hoveredIndex;
    qint64 m_currentMonthTotal;

    void calculateLayout();
};

#endif // XPCHAIN_QT_STAKINGREWARDCHARTWIDGET_H
