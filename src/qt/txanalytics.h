#ifndef XPCHAIN_QT_TXANALYTICS_H
#define XPCHAIN_QT_TXANALYTICS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QList>
#include <QPair>
#include <QMap>

class TxAnalytics : public QObject
{
    Q_OBJECT

public:
    static TxAnalytics& getInstance();

    // 초기화: 데이터 디렉토리 경로를 전달받아 tx_metadata.db와 인메모리 DB를 구성함
    bool init(const QString& dataDir);

    // 인메모리 기록 초기화 (동적 갱신 시 필요)
    void clearHistory();

    // 트랜잭션 기록 인메모리 DB에 추가
    bool addHistory(const QString& txid, qint64 time, int type, double amount, const QString& address);

    // 태그/메모 설정 및 가져오기
    bool setTag(const QString& txid, const QString& tag);
    QString getTag(const QString& txid);

    // 태그로 트랜잭션 ID 검색
    QList<QString> searchTxIdsByTag(const QString& tagQuery);

    // 월간 채굴 보상 통계 반환 (YYYY-MM, 합산금액)
    QList<QPair<QString, double>> getMonthlyMiningRewards();

private:
    TxAnalytics(QObject* parent = nullptr);
    ~TxAnalytics();

    TxAnalytics(const TxAnalytics&) = delete;
    TxAnalytics& operator=(const TxAnalytics&) = delete;

    QSqlDatabase m_memDb;
    QSqlDatabase m_fileDb;
    bool m_initialized;
};

#endif // XPCHAIN_QT_TXANALYTICS_H
