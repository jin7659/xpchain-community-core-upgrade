// Copyright (c) 2011-2020 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/assetpiechart.h>

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QLocale>
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {
struct Slice {
    qint64 amount;
    double angle;
    QColor color;
    QString name;
};

constexpr int kLegendReserve = 52;
} // namespace

AssetPieChart::AssetPieChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}

void AssetPieChart::setBalances(qint64 available, qint64 pending, qint64 immature, qint64 watchOnly, const QString& totalStr)
{
    m_available = available;
    m_pending = pending;
    m_immature = immature;
    m_watchOnly = watchOnly;
    m_totalStr = totalStr;
    update();
}

void AssetPieChart::calculateAngles(double& availableAngle, double& pendingAngle, double& immatureAngle, double& watchOnlyAngle) const
{
    qint64 total = m_available + m_pending + m_immature + m_watchOnly;
    if (total <= 0) {
        availableAngle = 0.0;
        pendingAngle = 0.0;
        immatureAngle = 0.0;
        watchOnlyAngle = 0.0;
        return;
    }

    int activeCount = 0;
    if (m_available > 0) activeCount++;
    if (m_pending > 0) activeCount++;
    if (m_immature > 0) activeCount++;
    if (m_watchOnly > 0) activeCount++;

    // Tiny gaps between slices for readability; keep a minimum arc so small slices stay visible.
    const double gapAngle = (activeCount > 1) ? 2.0 : 0.0;
    const double MIN_ANGLE = 5.0;
    double remainingAngle = 360.0 - (activeCount * (MIN_ANGLE + gapAngle));
    if (remainingAngle < 0.0) remainingAngle = 0.0;

    auto slice = [&](qint64 amount) -> double {
        return amount > 0 ? (MIN_ANGLE + remainingAngle * amount / total) : 0.0;
    };

    availableAngle = slice(m_available);
    pendingAngle = slice(m_pending);
    immatureAngle = slice(m_immature);
    watchOnlyAngle = slice(m_watchOnly);
}

void AssetPieChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int margin = 12;
    const int chartAreaHeight = height() - kLegendReserve;
    int size = std::min(width(), chartAreaHeight) - margin * 2;
    if (size <= 0) return;

    QRectF rect((width() - size) / 2.0, (chartAreaHeight - size) / 2.0, size, size);

    qint64 total = m_available + m_pending + m_immature + m_watchOnly;

    int penWidth = static_cast<int>(size * 0.11);
    if (penWidth < 10) penWidth = 10;
    if (penWidth > 22) penWidth = 22;

    rect.adjust(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

    // Soft track behind the donut
    {
        QPen track(QColor(255, 255, 255, 18), penWidth, Qt::SolidLine, Qt::FlatCap);
        painter.setPen(track);
        painter.drawArc(rect, 0, 360 * 16);
    }

    QVector<Slice> slices;
    if (total > 0) {
        double availableAngle = 0.0;
        double pendingAngle = 0.0;
        double immatureAngle = 0.0;
        double watchOnlyAngle = 0.0;
        calculateAngles(availableAngle, pendingAngle, immatureAngle, watchOnlyAngle);

        if (m_available > 0)
            slices.append({m_available, availableAngle, QColor("#2bbbad"), tr("Available")});
        if (m_pending > 0)
            slices.append({m_pending, pendingAngle, QColor("#e0a04a"), tr("Pending")});
        if (m_immature > 0)
            slices.append({m_immature, immatureAngle, QColor("#7f93a1"), tr("Immature")});
        if (m_watchOnly > 0)
            slices.append({m_watchOnly, watchOnlyAngle, QColor("#3d8ec4"), tr("Watch-only")});

        int startAngle = 90 * 16;
        const double gap = (slices.size() > 1) ? 2.0 : 0.0;
        for (const Slice& s : slices) {
            QPen pen(s.color, penWidth, Qt::SolidLine, Qt::FlatCap);
            painter.setPen(pen);
            const int span = static_cast<int>(-s.angle * 16);
            painter.drawArc(rect, startAngle, span);
            startAngle += span - static_cast<int>(gap * 16);
        }
    }

    // Center caption + total
    {
        QRectF textRect = rect.adjusted(penWidth, penWidth, -penWidth, -penWidth);
        painter.setPen(QColor(160, 170, 180));
        QFont caption = painter.font();
        caption.setBold(false);
        caption.setPointSize(std::max(8, static_cast<int>(size * 0.045)));
        painter.setFont(caption);
        painter.drawText(textRect.adjusted(0, textRect.height() * 0.18, 0, 0),
                         Qt::AlignHCenter | Qt::AlignTop, tr("Total"));

        painter.setPen(QColor("#f2f4f6"));
        QFont amount = painter.font();
        amount.setBold(true);
        int fontSize = static_cast<int>(size * 0.075);
        if (m_totalStr.length() > 14) fontSize = static_cast<int>(size * 0.055);
        if (m_totalStr.length() > 20) fontSize = static_cast<int>(size * 0.045);
        amount.setPointSize(std::max(9, fontSize));
        painter.setFont(amount);
        painter.drawText(textRect.adjusted(0, textRect.height() * 0.28, 0, -textRect.height() * 0.08),
                         Qt::AlignCenter | Qt::TextWordWrap, m_totalStr.isEmpty() ? QStringLiteral("—") : m_totalStr);
    }

    // Compact legend under the donut
    {
        const int legendTop = chartAreaHeight + 4;
        QFont legendFont = painter.font();
        legendFont.setBold(false);
        legendFont.setPointSize(8);
        painter.setFont(legendFont);
        QFontMetrics fm(legendFont);

        struct LegendItem { QColor color; QString label; };
        QVector<LegendItem> legend = {
            {QColor("#2bbbad"), tr("Available")},
            {QColor("#e0a04a"), tr("Pending")},
            {QColor("#7f93a1"), tr("Immature")},
            {QColor("#3d8ec4"), tr("Watch-only")},
        };

        int x = margin;
        const int y = legendTop + 6;
        const int swatch = 8;
        for (const LegendItem& item : legend) {
            painter.setBrush(item.color);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(QRect(x, y, swatch, swatch), 2, 2);
            x += swatch + 5;
            painter.setPen(QColor(170, 178, 186));
            painter.drawText(x, y + swatch, item.label);
            x +=
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
                fm.horizontalAdvance(item.label)
#else
                fm.width(item.label)
#endif
                + 14;
            if (x > width() - margin) break;
        }

        if (total <= 0) {
            painter.setPen(QColor(120, 128, 136));
            painter.drawText(QRect(margin, legendTop + 22, width() - margin * 2, 18),
                             Qt::AlignLeft | Qt::AlignVCenter, tr("No balances to chart yet"));
        }
    }
}

QSize AssetPieChart::sizeHint() const
{
    return QSize(200, 236);
}

QSize AssetPieChart::minimumSizeHint() const
{
    return QSize(160, 200);
}

void AssetPieChart::mouseMoveEvent(QMouseEvent *event)
{
    qint64 total = m_available + m_pending + m_immature + m_watchOnly;
    if (total <= 0) {
        QToolTip::hideText();
        return;
    }

    const int margin = 12;
    const int chartAreaHeight = height() - kLegendReserve;
    int size = std::min(width(), chartAreaHeight) - margin * 2;
    if (size <= 0) {
        QToolTip::hideText();
        return;
    }

    QRectF rect((width() - size) / 2.0, (chartAreaHeight - size) / 2.0, size, size);
    int penWidth = static_cast<int>(size * 0.11);
    if (penWidth < 10) penWidth = 10;
    if (penWidth > 22) penWidth = 22;
    rect.adjust(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

    QPointF center = rect.center();
    QPointF pos = event->pos();

    double dx = pos.x() - center.x();
    double dy = pos.y() - center.y();
    double distance = std::sqrt(dx * dx + dy * dy);

    double R = rect.width() / 2.0;
    double R_in = R - penWidth / 2.0;
    double R_out = R + penWidth / 2.0;

    if (distance < R_in || distance > R_out) {
        QToolTip::hideText();
        return;
    }

    double angle_rad = std::atan2(-dy, dx);
    double angle_deg = angle_rad * 180.0 / M_PI;
    if (angle_deg < 0) angle_deg += 360.0;
    double angle_from_12 = 90.0 - angle_deg;
    if (angle_from_12 < 0) angle_from_12 += 360.0;

    double availableAngle = 0.0;
    double pendingAngle = 0.0;
    double immatureAngle = 0.0;
    double watchOnlyAngle = 0.0;
    calculateAngles(availableAngle, pendingAngle, immatureAngle, watchOnlyAngle);

    QString sectionName;
    qint64 balance = 0;
    double start = 0.0;
    const double gap = 2.0;

    auto hit = [&](qint64 amount, double angle, const QString& name) {
        if (amount <= 0 || !sectionName.isEmpty()) {
            if (amount > 0) start += angle + gap;
            return;
        }
        double end = start + angle;
        if (angle_from_12 >= start && angle_from_12 < end) {
            sectionName = name;
            balance = amount;
        }
        start = end + gap;
    };

    hit(m_available, availableAngle, tr("Available"));
    hit(m_pending, pendingAngle, tr("Pending"));
    hit(m_immature, immatureAngle, tr("Immature"));
    hit(m_watchOnly, watchOnlyAngle, tr("Watch-only"));

    if (!sectionName.isEmpty()) {
        QLocale locale = QLocale::system();
        double dCoins = (double)balance / 10000.0;
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
