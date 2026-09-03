// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/stakingrewardchartwidget.h>
#include <qt/walletmodel.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionrecord.h>
#include <qt/xpchainunits.h>
#include <qt/optionsmodel.h>
#include <amount.h>

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDateTime>
#include <QLocale>
#include <QFontMetrics>
#include <cmath>

StakingRewardChartWidget::StakingRewardChartWidget(QWidget *parent)
    : QWidget(parent),
      walletModel(nullptr),
      m_hoveredIndex(-1),
      m_currentMonthTotal(0)
{
    setMouseTracking(true);
    setMinimumHeight(185);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

StakingRewardChartWidget::~StakingRewardChartWidget()
{
}

void StakingRewardChartWidget::setWalletModel(WalletModel *model)
{
    if (walletModel == model) return;
    walletModel = model;

    if (walletModel) {
        TransactionTableModel *ttm = walletModel->getTransactionTableModel();
        if (ttm) {
            connect(ttm, &QAbstractItemModel::rowsInserted, this, &StakingRewardChartWidget::updateData);
            connect(ttm, &QAbstractItemModel::modelReset, this, &StakingRewardChartWidget::updateData);
            connect(ttm, &QAbstractItemModel::dataChanged, this, &StakingRewardChartWidget::updateData);
        }
    }
    updateData();
}

void StakingRewardChartWidget::updateData()
{
    m_monthlyData.clear();
    m_currentMonthTotal = 0;

    if (!walletModel || !walletModel->getTransactionTableModel()) {
        update();
        return;
    }

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    int rows = ttm->rowCount(QModelIndex());

    // 최근 6개월 슬롯 사전 준비
    QDateTime now = QDateTime::currentDateTime();
    QMap<QString, MonthlyStakingStats> statsMap;

    for (int i = 5; i >= 0; --i) {
        QDateTime dt = now.addMonths(-i);
        QString key = dt.toString("yyyy-MM");
        QString label = QLocale().monthName(dt.date().month(), QLocale::ShortFormat);
        MonthlyStakingStats stat;
        stat.monthKey = key;
        stat.monthLabel = label;
        stat.totalAmount = 0;
        stat.stakeCount = 0;
        statsMap[key] = stat;
    }

    // 트랜잭션 순회하여 스테이킹 보상(Generated) 집계
    for (int r = 0; r < rows; ++r) {
        QModelIndex idx = ttm->index(r, 0);
        int type = ttm->data(idx, TransactionTableModel::TypeRole).toInt();
        if (type == TransactionRecord::Generated) {
            QDateTime date = ttm->data(idx, TransactionTableModel::DateRole).toDateTime();
            qint64 amount = ttm->data(idx, TransactionTableModel::AmountRole).toLongLong();
            QString key = date.toString("yyyy-MM");

            if (statsMap.contains(key)) {
                statsMap[key].totalAmount += amount;
                statsMap[key].stakeCount += 1;
            }
        }
    }

    QString currentKey = now.toString("yyyy-MM");
    if (statsMap.contains(currentKey)) {
        m_currentMonthTotal = statsMap[currentKey].totalAmount;
    }

    for (int i = 5; i >= 0; --i) {
        QDateTime dt = now.addMonths(-i);
        QString key = dt.toString("yyyy-MM");
        if (statsMap.contains(key)) {
            m_monthlyData.append(statsMap[key]);
        }
    }

    calculateLayout();
    update();
}

void StakingRewardChartWidget::calculateLayout()
{
    m_barRects.clear();
    if (m_monthlyData.isEmpty()) return;

    const int paddingLeft = 16;
    const int paddingRight = 16;
    const int paddingTop = 54;
    const int paddingBottom = 26;

    const int w = width();
    const int h = height();
    const int chartHeight = h - paddingTop - paddingBottom;
    const int count = m_monthlyData.size();

    if (count == 0 || chartHeight <= 0) return;

    qint64 maxVal = 0;
    for (const auto &stat : m_monthlyData) {
        if (stat.totalAmount > maxVal) maxVal = stat.totalAmount;
    }
    if (maxVal <= 0) maxVal = COIN; // 기본 스케일

    const int availableWidth = w - paddingLeft - paddingRight;
    const int slotWidth = availableWidth / count;
    const int barWidth = qBound(12, slotWidth - 16, 32);

    for (int i = 0; i < count; ++i) {
        const qint64 val = m_monthlyData[i].totalAmount;
        int barHeight = static_cast<int>((static_cast<double>(val) / maxVal) * chartHeight);
        if (val > 0 && barHeight < 4) barHeight = 4;

        int slotCenterX = paddingLeft + i * slotWidth + slotWidth / 2;
        int x = slotCenterX - barWidth / 2;
        int y = h - paddingBottom - barHeight;

        m_barRects.append(QRect(x, y, barWidth, barHeight));
    }
}

void StakingRewardChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // 카드 배경 (모던 다크/세미트랜스패런트 테마)
    QColor bgColor(22, 27, 34, 220);
    QColor borderColor(48, 54, 61);
    painter.setBrush(bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), 8, 8);

    // 차트 제목
    painter.setPen(QColor(230, 237, 243));
    QFont fontTitle = font();
    fontTitle.setPointSize(10);
    fontTitle.setBold(true);
    painter.setFont(fontTitle);
    painter.drawText(16, 26, tr("Monthly Staking Rewards"));

    // 우측 이번 달 요약
    int unit = walletModel && walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : XPChainUnits::XPC;
    QString currentStr = tr("This month: %1").arg(XPChainUnits::formatWithUnit(unit, m_currentMonthTotal, false, XPChainUnits::separatorAlways));
    painter.setPen(QColor(88, 166, 255));
    QFont fontSub = font();
    fontSub.setPointSize(9);
    fontSub.setBold(true);
    painter.setFont(fontSub);
    painter.drawText(QRect(w - 240, 12, 224, 20), Qt::AlignRight | Qt::AlignVCenter, currentStr);

    calculateLayout();

    // 데이터가 전혀 없는 경우
    bool hasAnyData = false;
    for (const auto &stat : m_monthlyData) {
        if (stat.totalAmount > 0) {
            hasAnyData = true;
            break;
        }
    }

    const int paddingBottom = 26;
    const int baselineY = h - paddingBottom;

    // 베이스라인 가이드선
    painter.setPen(QPen(QColor(48, 54, 61), 1));
    painter.drawLine(14, baselineY, w - 14, baselineY);

    if (!hasAnyData) {
        painter.setPen(QColor(139, 148, 158));
        QFont fontEmpty = font();
        fontEmpty.setPointSize(9);
        painter.setFont(fontEmpty);
        painter.drawText(QRect(16, 54, w - 32, h - 90), Qt::AlignCenter,
                         tr("No staking rewards recorded yet in the past 6 months."));
    }

    // 막대 차트 및 X축 라벨 렌더링
    for (int i = 0; i < m_barRects.size(); ++i) {
        const QRect &rect = m_barRects[i];
        const auto &stat = m_monthlyData[i];
        bool isHovered = (i == m_hoveredIndex);

        // X축 월 라벨
        painter.setPen(isHovered ? QColor(88, 166, 255) : QColor(139, 148, 158));
        QFont fontX = font();
        fontX.setPointSize(8);
        fontX.setBold(isHovered);
        painter.setFont(fontX);
        painter.drawText(QRect(rect.x() - 16, baselineY + 5, rect.width() + 32, 16),
                         Qt::AlignCenter, stat.monthLabel);

        if (rect.height() > 0) {
            // 테크 블루 그라데이션 막대
            QLinearGradient barGradient(rect.topLeft(), rect.bottomLeft());
            if (isHovered) {
                barGradient.setColorAt(0.0, QColor(88, 166, 255));
                barGradient.setColorAt(1.0, QColor(31, 111, 235));
            } else {
                barGradient.setColorAt(0.0, QColor(56, 139, 253));
                barGradient.setColorAt(1.0, QColor(31, 111, 235, 190));
            }

            QPainterPath barPath;
            barPath.addRoundedRect(rect, 4, 4);
            painter.fillPath(barPath, barGradient);
        } else {
            // 실적이 없는 달은 작은 점(dot) 표시
            painter.setBrush(QColor(48, 54, 61));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPoint(rect.x() + rect.width() / 2, baselineY - 2), 2, 2);
        }
    }

    // 호버 시 그래프 위에 선명하게 뜨는 툴팁 카드 렌더링
    if (m_hoveredIndex >= 0 && m_hoveredIndex < m_barRects.size()) {
        const QRect &rect = m_barRects[m_hoveredIndex];
        const auto &stat = m_monthlyData[m_hoveredIndex];

        QString amountStr = XPChainUnits::formatWithUnit(unit, stat.totalAmount, false, XPChainUnits::separatorAlways);
        QString tipLine1 = QString("%1 (%2 %3)").arg(stat.monthKey).arg(stat.stakeCount).arg(tr("stakes"));
        QString tipLine2 = QString("+%1").arg(amountStr);

        QFont fontTip = font();
        fontTip.setPointSize(8);
        QFontMetrics fm(fontTip);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        int tipW = qMax(fm.horizontalAdvance(tipLine1), fm.horizontalAdvance(tipLine2)) + 18;
#else
        int tipW = qMax(fm.width(tipLine1), fm.width(tipLine2)) + 18;
#endif
        int tipH = 38;

        int tipX = rect.center().x() - tipW / 2;
        tipX = qBound(8, tipX, w - tipW - 8);
        // 그래프 위로 항상 선명하게 표시 (바닥으로 처박혀 잘리는 현상 방지)
        int tipY = qMax(34, rect.top() - tipH - 6);

        QRect tipRect(tipX, tipY, tipW, tipH);
        painter.setBrush(QColor(13, 17, 23, 245));
        painter.setPen(QPen(QColor(56, 139, 253), 1));
        painter.drawRoundedRect(tipRect, 6, 6);

        painter.setPen(QColor(201, 209, 217));
        painter.setFont(fontTip);
        painter.drawText(QRect(tipX + 8, tipY + 4, tipW - 16, 14), Qt::AlignLeft, tipLine1);

        painter.setPen(QColor(88, 166, 255));
        fontTip.setBold(true);
        painter.setFont(fontTip);
        painter.drawText(QRect(tipX + 8, tipY + 18, tipW - 16, 14), Qt::AlignLeft, tipLine2);
    }
}

void StakingRewardChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    int oldHovered = m_hoveredIndex;
    m_hoveredIndex = -1;

    for (int i = 0; i < m_barRects.size(); ++i) {
        QRect hitArea = m_barRects[i].adjusted(-8, -10, 8, 20);
        if (hitArea.contains(event->pos())) {
            m_hoveredIndex = i;
            break;
        }
    }

    if (m_hoveredIndex != oldHovered) {
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void StakingRewardChartWidget::leaveEvent(QEvent *event)
{
    if (m_hoveredIndex != -1) {
        m_hoveredIndex = -1;
        update();
    }
    QWidget::leaveEvent(event);
}
