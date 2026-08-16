#include <qt/transactionanalyticswidget.h>
#include <qt/txanalytics.h>

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDateTime>
#include <QtGlobal>

TransactionAnalyticsWidget::TransactionAnalyticsWidget(QWidget* parent)
    : QWidget(parent), m_hoveredBarIndex(-1)
{
    setMouseTracking(true);
    setMinimumSize(320, 220);
    setAutoFillBackground(false);
    updateData();
}

TransactionAnalyticsWidget::~TransactionAnalyticsWidget()
{
}

void TransactionAnalyticsWidget::updateData()
{
    m_rewardData = TxAnalytics::getInstance().getMonthlyMiningRewards();
    calculateLayout();
    update();
}

void TransactionAnalyticsWidget::calculateLayout()
{
    m_barRects.clear();
    if (m_rewardData.isEmpty()) return;

    const int paddingLeft = 48;
    const int paddingRight = 16;
    const int paddingTop = 44;
    const int paddingBottom = 36;

    const int count = m_rewardData.size();
    const int barWidth = 16;
    const int spacing = 14;

    const int calculatedWidth = paddingLeft + paddingRight + count * (barWidth + spacing) - spacing + 12;
    setMinimumWidth(qMax(calculatedWidth, 320));

    const int h = height();
    const int chartHeight = h - paddingTop - paddingBottom;

    double maxVal = 0.0;
    for (const auto& pair : m_rewardData) {
        if (pair.second > maxVal) maxVal = pair.second;
    }
    if (maxVal < 1.0) maxVal = 1.0;

    for (int i = 0; i < count; ++i) {
        const double val = m_rewardData[i].second;
        int barHeight = static_cast<int>((val / maxVal) * chartHeight);
        if (val > 0.0 && barHeight < 3) barHeight = 3;

        const int x = paddingLeft + i * (barWidth + spacing);
        const int y = h - paddingBottom - barHeight;

        BarRect br;
        br.rect = QRect(x, y, barWidth, barHeight);
        br.month = m_rewardData[i].first;
        br.amount = val;
        m_barRects.append(br);
    }
}

void TransactionAnalyticsWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // Quiet panel — match app chrome, avoid heavy glass/card chrome
    painter.setBrush(QColor(26, 26, 26));
    painter.setPen(QPen(QColor(45, 45, 45), 1));
    painter.drawRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), 8, 8);

    // Title
    painter.setPen(QColor(232, 234, 237));
    QFont fontTitle = font();
    fontTitle.setBold(true);
    fontTitle.setPointSize(10);
    painter.setFont(fontTitle);
    painter.drawText(16, 26, tr("Monthly staking rewards"));

    painter.setPen(QColor(120, 128, 136));
    QFont fontSub = font();
    fontSub.setPointSize(8);
    fontSub.setBold(false);
    painter.setFont(fontSub);
    painter.drawText(16, 40, tr("PoS rewards by month"));

    if (m_rewardData.isEmpty()) {
        painter.setPen(QColor(120, 128, 136));
        QFont fontMsg = font();
        fontMsg.setPointSize(9);
        painter.setFont(fontMsg);
        painter.drawText(QRect(16, 56, w - 32, h - 80), Qt::AlignCenter,
                         tr("No staking reward history yet"));
        return;
    }

    const int paddingLeft = 48;
    const int paddingRight = 16;
    const int paddingTop = 44;
    const int paddingBottom = 36;
    const int chartHeight = h - paddingTop - paddingBottom;

    double maxVal = 0.0;
    for (const auto& br : m_barRects) {
        if (br.amount > maxVal) maxVal = br.amount;
    }
    if (maxVal < 1.0) maxVal = 1.0;

    // Grid + Y labels
    QFont fontAxis = font();
    fontAxis.setPointSize(8);
    painter.setFont(fontAxis);

    const int gridLines = 4;
    for (int i = 0; i <= gridLines; ++i) {
        const int y = paddingTop + (chartHeight * i / gridLines);
        painter.setPen(QPen(QColor(255, 255, 255, 16), 1, Qt::SolidLine));
        painter.drawLine(paddingLeft, y, w - paddingRight, y);

        const double gridVal = maxVal * (gridLines - i) / gridLines;
        QString valText;
        if (gridVal >= 1000.0) {
            valText = QString("%1k").arg(gridVal / 1000.0, 0, 'f', 1);
        } else {
            valText = QString::number(gridVal, 'f', gridVal >= 10.0 ? 0 : 1);
        }
        painter.setPen(QColor(130, 138, 146));
        painter.drawText(QRect(4, y - 8, paddingLeft - 10, 16), Qt::AlignRight | Qt::AlignVCenter, valText);
    }

    // Bars
    for (int i = 0; i < m_barRects.size(); ++i) {
        const auto& br = m_barRects[i];
        const bool isHovered = (i == m_hoveredBarIndex);

        QLinearGradient barGrad(br.rect.left(), br.rect.top(), br.rect.left(), br.rect.bottom());
        if (isHovered) {
            barGrad.setColorAt(0, QColor("#4aa3d8"));
            barGrad.setColorAt(1, QColor("#106ba3"));
        } else {
            barGrad.setColorAt(0, QColor("#2f8ec4"));
            barGrad.setColorAt(1, QColor("#0d5a8a"));
        }

        painter.setBrush(barGrad);
        painter.setPen(Qt::NoPen);

        QPainterPath path;
        const int radius = qMin(br.rect.width() / 2, 4);
        path.moveTo(br.rect.bottomLeft());
        path.lineTo(br.rect.topLeft() + QPoint(0, radius));
        path.quadTo(br.rect.topLeft(), br.rect.topLeft() + QPoint(radius, 0));
        path.lineTo(br.rect.topRight() - QPoint(radius, 0));
        path.quadTo(br.rect.topRight(), br.rect.topRight() + QPoint(0, radius));
        path.lineTo(br.rect.bottomRight());
        path.closeSubpath();
        painter.drawPath(path);

        // Month label
        painter.setPen(QColor(130, 138, 146));
        QFont fontLabel = font();
        fontLabel.setPointSize(8);
        painter.setFont(fontLabel);
        QString labelStr = br.month;
        if (labelStr.contains(QLatin1Char('-'))) {
            labelStr = labelStr.split(QLatin1Char('-')).last();
        }
        painter.drawText(QRect(br.rect.left() - 8, h - paddingBottom + 6, br.rect.width() + 16, 18),
                         Qt::AlignCenter, labelStr);
    }

    // Hover tooltip
    if (m_hoveredBarIndex >= 0 && m_hoveredBarIndex < m_barRects.size()) {
        const auto& br = m_barRects[m_hoveredBarIndex];
        const QString text = QString("%1  ·  %2 XPC").arg(br.month).arg(br.amount, 0, 'f', 2);
        QFont tooltipFont = font();
        tooltipFont.setPointSize(8);
        tooltipFont.setBold(true);
        QFontMetrics fm(tooltipFont);
        const int textW =
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
            fm.horizontalAdvance(text)
#else
            fm.width(text)
#endif
            + 16;
        const int textH = 24;

        int tooltipX = m_mousePos.x() + 12;
        int tooltipY = m_mousePos.y() - 32;
        if (tooltipX + textW > w - 4) tooltipX = m_mousePos.x() - textW - 8;
        if (tooltipY < 4) tooltipY = 4;

        const QRect tooltipRect(tooltipX, tooltipY, textW, textH);
        painter.setBrush(QColor(18, 18, 18, 230));
        painter.setPen(QPen(QColor("#106ba3"), 1));
        painter.drawRoundedRect(tooltipRect, 4, 4);
        painter.setPen(QColor("#ffffff"));
        painter.setFont(tooltipFont);
        painter.drawText(tooltipRect, Qt::AlignCenter, text);
    }
}

void TransactionAnalyticsWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_mousePos = event->pos();
    const int oldHovered = m_hoveredBarIndex;
    m_hoveredBarIndex = -1;

    for (int i = 0; i < m_barRects.size(); ++i) {
        QRect checkRect = m_barRects[i].rect;
        checkRect.setTop(44);
        checkRect.setBottom(height() - 36);
        if (checkRect.contains(m_mousePos)) {
            m_hoveredBarIndex = i;
            break;
        }
    }

    if (m_hoveredBarIndex != oldHovered) {
        update();
    } else if (m_hoveredBarIndex >= 0) {
        update(); // keep tooltip following the cursor
    }
}

void TransactionAnalyticsWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (m_hoveredBarIndex != -1) {
        m_hoveredBarIndex = -1;
        update();
    }
}

void TransactionAnalyticsWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    calculateLayout();
}
