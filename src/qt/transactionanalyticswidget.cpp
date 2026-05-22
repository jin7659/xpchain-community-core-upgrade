#include <qt/transactionanalyticswidget.h>
#include <qt/txanalytics.h>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QDateTime>
#include <QDebug>

TransactionAnalyticsWidget::TransactionAnalyticsWidget(QWidget* parent)
    : QWidget(parent), m_hoveredBarIndex(-1)
{
    setMouseTracking(true);
    setMinimumSize(320, 240);
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

    int w = width();
    int h = height();

    // 여백 및 가이드라인 설정
    int paddingLeft = 55;
    int paddingRight = 20;
    int paddingTop = 40;
    int paddingBottom = 40;

    int chartWidth = w - paddingLeft - paddingRight;
    int chartHeight = h - paddingTop - paddingBottom;

    double maxVal = 0.0;
    for (const auto& pair : m_rewardData) {
        if (pair.second > maxVal) {
            maxVal = pair.second;
        }
    }
    if (maxVal < 1.0) maxVal = 100.0; // 최소 기준치 설정

    // 바 배치 계산
    int count = m_rewardData.size();
    int spacing = 12;
    int barWidth = (chartWidth - (spacing * (count - 1))) / count;
    if (barWidth < 4) barWidth = 4; // 최소 너비 보장

    for (int i = 0; i < count; ++i) {
        double val = m_rewardData[i].second;
        int barHeight = static_cast<int>((val / maxVal) * chartHeight);
        if (barHeight < 2) barHeight = 2; // 최소 2픽셀 표시

        int x = paddingLeft + i * (barWidth + spacing);
        int y = h - paddingBottom - barHeight;

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

    int w = width();
    int h = height();

    // 1. 미려한 모던 다크 카드 배경 드로잉 (글래스모피즘 효과 반투명 다크)
    QLinearGradient bgGrad(0, 0, 0, h);
    bgGrad.setColorAt(0, QColor(32, 33, 36, 240));
    bgGrad.setColorAt(1, QColor(24, 25, 28, 240));
    painter.setBrush(bgGrad);
    painter.setPen(QPen(QColor(60, 64, 67, 120), 1.5));
    painter.drawRoundedRect(QRect(1, 1, w - 2, h - 2), 16, 16);

    // 2. 타이틀 영역 드로잉
    painter.setPen(QColor(240, 240, 240));
    QFont fontTitle("Inter", 11, QFont::Bold);
    painter.setFont(fontTitle);
    painter.drawText(20, 28, tr("월간 채굴 수익 추이"));

    // 3. 데이터 부족 시 안내 문구 출력
    if (m_rewardData.isEmpty()) {
        painter.setPen(QColor(128, 134, 139));
        QFont fontMsg("Inter", 9, QFont::Normal);
        painter.setFont(fontMsg);
        painter.drawText(QRect(20, 50, w - 40, h - 90), Qt::AlignCenter, tr("채굴 데이터가 존재하지 않습니다."));
        return;
    }

    int paddingLeft = 55;
    int paddingRight = 20;
    int paddingTop = 40;
    int paddingBottom = 40;
    int chartHeight = h - paddingTop - paddingBottom;

    // Y축 가이드선 및 스케일 드로잉
    double maxVal = 0.0;
    for (const auto& br : m_barRects) {
        if (br.amount > maxVal) maxVal = br.amount;
    }
    if (maxVal < 1.0) maxVal = 100.0;

    painter.setPen(QPen(QColor(60, 64, 67, 80), 1, Qt::DashLine));
    QFont fontAxis("Inter", 8);
    painter.setFont(fontAxis);

    int gridLines = 4;
    for (int i = 0; i <= gridLines; ++i) {
        int y = paddingTop + (chartHeight * i / gridLines);
        // 가이드 점선
        painter.drawLine(paddingLeft, y, w - paddingRight, y);

        // 스케일 텍스트
        double gridVal = maxVal * (gridLines - i) / gridLines;
        QString valText = QString("%1").arg(gridVal, 0, 'f', 0);
        if (gridVal >= 1000) {
            valText = QString("%1k").arg(gridVal / 1000.0, 0, 'f', 1);
        }
        painter.setPen(QColor(154, 160, 166));
        painter.drawText(QRect(5, y - 8, paddingLeft - 12, 16), Qt::AlignRight | Qt::AlignVCenter, valText);
        painter.setPen(QPen(QColor(60, 64, 67, 80), 1, Qt::DashLine));
    }

    // 4. 바 차트 기둥 그리기
    for (int i = 0; i < m_barRects.size(); ++i) {
        const auto& br = m_barRects[i];
        bool isHovered = (i == m_hoveredBarIndex);

        // 그라데이션 설정 (스포티한 네온 오렌지 매칭)
        QLinearGradient barGrad(br.rect.left(), br.rect.top(), br.rect.left(), br.rect.bottom());
        if (isHovered) {
            barGrad.setColorAt(0, QColor(255, 150, 0)); // 밝은 네온
            barGrad.setColorAt(1, QColor(255, 60, 0));
        } else {
            barGrad.setColorAt(0, QColor(255, 123, 0)); // 메인 스포티 오렌지
            barGrad.setColorAt(1, QColor(255, 51, 0));
        }

        painter.setBrush(barGrad);
        if (isHovered) {
            painter.setPen(QPen(QColor(255, 200, 150), 1.5));
        } else {
            painter.setPen(Qt::NoPen);
        }

        // 윗부분만 둥글게 다듬는 경로(Path) 구성
        QPainterPath path;
        int radius = qMin(br.rect.width() / 2, 6);
        path.addRoundedRect(br.rect, radius, radius);
        // 아래쪽 둥근 모서리는 보정 (아랫면은 평평하게)
        if (br.rect.height() > radius) {
            QRect bottomPart(br.rect.left(), br.rect.bottom() - radius, br.rect.width(), radius);
            path.addRect(bottomPart);
        }
        painter.drawPath(path);

        // 월 라벨 표시
        painter.setPen(QColor(154, 160, 166));
        QFont fontLabel("Inter", 8);
        painter.setFont(fontLabel);
        // "2026-05" -> "05월" 포맷 간소화
        QString labelStr = br.month;
        if (labelStr.contains('-')) {
            labelStr = labelStr.split('-').last() + tr("월");
        }
        painter.drawText(QRect(br.rect.left() - 10, h - paddingBottom + 5, br.rect.width() + 20, 20),
                         Qt::AlignCenter, labelStr);
    }

    // 5. 마우스 호버 상태일 때 툴팁 오버레이 카드 렌더링
    if (m_hoveredBarIndex >= 0 && m_hoveredBarIndex < m_barRects.size()) {
        const auto& br = m_barRects[m_hoveredBarIndex];

        // 툴팁 상자 계산
        QString text = QString("%1: %2 XPC").arg(br.month).arg(br.amount, 0, 'f', 2);
        QFont tooltipFont("Inter", 8, QFont::Bold);
        QFontMetrics fm(tooltipFont);
        int textW = fm.horizontalAdvance(text) + 20;
        int textH = 28;

        int tooltipX = m_mousePos.x() + 15;
        int tooltipY = m_mousePos.y() - 35;

        // 경계선 이탈 방지
        if (tooltipX + textW > w) tooltipX = m_mousePos.x() - textW - 10;
        if (tooltipY < 5) tooltipY = 5;

        QRect tooltipRect(tooltipX, tooltipY, textW, textH);

        painter.setBrush(QColor(36, 37, 40, 235));
        painter.setPen(QPen(QColor(255, 123, 0, 200), 1.5));
        painter.drawRoundedRect(tooltipRect, 6, 6);

        painter.setPen(QColor(255, 255, 255));
        painter.setFont(tooltipFont);
        painter.drawText(tooltipRect, Qt::AlignCenter, text);
    }
}

void TransactionAnalyticsWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_mousePos = event->pos();
    int oldHovered = m_hoveredBarIndex;
    m_hoveredBarIndex = -1;

    for (int i = 0; i < m_barRects.size(); ++i) {
        // 호버 영역 판정: 위아래 여백을 넓혀 기둥 주변 마우스 위치에서도 호버가 잘 작동하게 함
        QRect checkRect = m_barRects[i].rect;
        checkRect.setTop(40);
        checkRect.setBottom(height() - 40);

        if (checkRect.contains(m_mousePos)) {
            m_hoveredBarIndex = i;
            break;
        }
    }

    if (m_hoveredBarIndex != oldHovered) {
        update();
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
