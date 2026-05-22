// Copyright (c) 2011-2020 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_ASSETPIECHART_H
#define XPCHAIN_QT_ASSETPIECHART_H

#include <QWidget>

class AssetPieChart : public QWidget
{
    Q_OBJECT
public:
    explicit AssetPieChart(QWidget *parent = nullptr);

    void setBalances(qint64 available, qint64 pending, qint64 immature, const QString& totalStr);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;


private:
    qint64 m_available = 0;
    qint64 m_pending = 0;
    qint64 m_immature = 0;
    QString m_totalStr;
};

#endif // XPCHAIN_QT_ASSETPIECHART_H
