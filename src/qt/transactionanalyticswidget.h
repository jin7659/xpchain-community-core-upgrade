#ifndef XPCHAIN_QT_TRANSACTIONANALYTICSWIDGET_H
#define XPCHAIN_QT_TRANSACTIONANALYTICSWIDGET_H

#include <QWidget>
#include <QList>
#include <QPair>
#include <QString>
#include <QPoint>

class TransactionAnalyticsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionAnalyticsWidget(QWidget* parent = nullptr);
    ~TransactionAnalyticsWidget();

    // 차트 데이터 수동 갱신 유도
    void updateData();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct BarRect {
        QRect rect;
        QString month;
        double amount;
    };

    QList<QPair<QString, double>> m_rewardData;
    QList<BarRect> m_barRects;
    int m_hoveredBarIndex;
    QPoint m_mousePos;

    void calculateLayout();
};

#endif // XPCHAIN_QT_TRANSACTIONANALYTICSWIDGET_H
