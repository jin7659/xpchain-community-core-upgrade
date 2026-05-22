// Copyright (c) 2011-2020 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/assetpiechart.h>

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QLocale>
#include <algorithm>
#include <cmath>


AssetPieChart::AssetPieChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}


void AssetPieChart::setBalances(qint64 available, qint64 pending, qint64 immature, const QString& totalStr)
{
    m_available = available;
    m_pending = pending;
    m_immature = immature;
    m_totalStr = totalStr;
    update();
}

void AssetPieChart::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 여백 및 가용 크기 계산
    int margin = 16;
    int size = std::min(width(), height()) - margin * 2;
    if (size <= 0) return;

    QRectF rect((width() - size) / 2.0, (height() - size) / 2.0, size, size);

    qint64 total = m_available + m_pending + m_immature;

    // 도넛 링의 두께 (지름에 비례하여 깔끔하게 설정)
    int penWidth = size * 0.12;
    if (penWidth < 12) penWidth = 12;

    // 약간 안쪽으로 rect 크기 조절 (펜 두께의 절반만큼 줄여서 그리기 범위 안으로 제한)
    rect.adjust(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

    if (total == 0) {
        // 비어있는 상태의 회색 도넛
        QPen pen(QColor("#37474f"), penWidth, Qt::SolidLine, Qt::FlatCap);
        painter.setPen(pen);
        painter.drawArc(rect, 0, 360 * 16);
    } else {
        double availableAngle = 360.0 * m_available / total;
        double pendingAngle = 360.0 * m_pending / total;
        double immatureAngle = 360.0 * m_immature / total;

        int startAngle = 90 * 16; // 12시 방향에서 시작

        // 1. Available (Teal)
        if (m_available > 0) {
            QPen pen(QColor("#26a69a"), penWidth, Qt::SolidLine, Qt::FlatCap);
            painter.setPen(pen);
            int spanAngle = -availableAngle * 16;
            painter.drawArc(rect, startAngle, spanAngle);
            startAngle += spanAngle;
        }

        // 2. Pending (Orange)
        if (m_pending > 0) {
            QPen pen(QColor("#ffb74d"), penWidth, Qt::SolidLine, Qt::FlatCap);
            painter.setPen(pen);
            int spanAngle = -pendingAngle * 16;
            painter.drawArc(rect, startAngle, spanAngle);
            startAngle += spanAngle;
        }

        // 3. Immature (Grey/Blue-grey)
        if (m_immature > 0) {
            QPen pen(QColor("#90a4ae"), penWidth, Qt::SolidLine, Qt::FlatCap);
            painter.setPen(pen);
            int spanAngle = -immatureAngle * 16;
            painter.drawArc(rect, startAngle, spanAngle);
            startAngle += spanAngle;
        }
    }

    // 중앙의 총 잔액 텍스트 그리기
    if (!m_totalStr.isEmpty()) {
        painter.setPen(QColor("#ffffff"));
        
        // 폰트 크기 및 비율 세부 튜닝
        QFont font = painter.font();
        font.setBold(true);
        // 글자 길이에 맞춰 적응형 폰트 크기 조절
        int fontSize = size * 0.08;
        if (m_totalStr.length() > 15) {
            fontSize = size * 0.06;
        }
        font.setPointSize(std::max(9, fontSize));
        painter.setFont(font);

        // 총액 표시
        painter.drawText(rect, Qt::AlignCenter, m_totalStr);
    }
}

QSize AssetPieChart::sizeHint() const
{
    return QSize(180, 180);
}

QSize AssetPieChart::minimumSizeHint() const
{
    return QSize(140, 140);
}

void AssetPieChart::mouseMoveEvent(QMouseEvent *event)
{
    qint64 total = m_available + m_pending + m_immature;
    if (total <= 0) {
        QToolTip::hideText();
        return;
    }

    int margin = 16;
    int size = std::min(width(), height()) - margin * 2;
    if (size <= 0) {
        QToolTip::hideText();
        return;
    }

    QRectF rect((width() - size) / 2.0, (height() - size) / 2.0, size, size);
    int penWidth = size * 0.12;
    if (penWidth < 12) penWidth = 12;

    rect.adjust(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

    QPointF center = rect.center();
    QPointF pos = event->pos();
    
    // 중심점으로부터의 거리 계산
    double dx = pos.x() - center.x();
    double dy = pos.y() - center.y();
    double distance = std::sqrt(dx * dx + dy * dy);

    double R = rect.width() / 2.0;
    double R_in = R - penWidth / 2.0;
    double R_out = R + penWidth / 2.0;

    // 도넛 링 영역 내부가 아닌 경우 툴팁 숨김
    if (distance < R_in || distance > R_out) {
        QToolTip::hideText();
        return;
    }

    // 각도 계산 (12시 방향 기준 시계방향 0~360도)
    double angle_rad = std::atan2(-dy, dx);
    double angle_deg = angle_rad * 180.0 / M_PI;
    if (angle_deg < 0) {
        angle_deg += 360.0;
    }
    double angle_from_12 = 90.0 - angle_deg;
    if (angle_from_12 < 0) {
        angle_from_12 += 360.0;
    }

    double availableAngle = 360.0 * m_available / total;
    double pendingAngle = 360.0 * m_pending / total;
    double immatureAngle = 360.0 * m_immature / total;

    QString sectionName;
    qint64 balance = 0;
    
    double start = 0.0;
    if (m_available > 0) {
        double end = start + availableAngle;
        if (angle_from_12 >= start && angle_from_12 < end) {
            sectionName = tr("Available");
            balance = m_available;
        }
        start = end;
    }
    if (m_pending > 0 && sectionName.isEmpty()) {
        double end = start + pendingAngle;
        if (angle_from_12 >= start && angle_from_12 < end) {
            sectionName = tr("Pending");
            balance = m_pending;
        }
        start = end;
    }
    if (m_immature > 0 && sectionName.isEmpty()) {
        double end = start + immatureAngle;
        if (angle_from_12 >= start && angle_from_12 < end) {
            sectionName = tr("Immature");
            balance = m_immature;
        }
    }

    if (!sectionName.isEmpty()) {
        QLocale locale = QLocale::system();
        double dCoins = (double)balance / 100000000.0;
        double percent = 100.0 * balance / total;
        
        QString tooltipText = QString("<b>%1</b><br/>%2 XPC (%3%)")
            .arg(sectionName)
            .arg(locale.toString(dCoins, 'f', 4))
            .arg(locale.toString(percent, 'f', 1));
            
        QToolTip::showText(event->globalPos(), tooltipText, this);
    } else {
        QToolTip::hideText();
    }
}

void AssetPieChart::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    QToolTip::hideText();
}

