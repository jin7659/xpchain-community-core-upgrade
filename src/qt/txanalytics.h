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
    static void destroy();

    // 초기화: 데이터 디렉토리 경로를 전달받아 tx_metadata.db를 구성함
    bool init(const QString& dataDir);

    // 태그/메모 설정 및 가져오기
    bool setTag(const QString& txid, const QString& tag);
    QString getTag(const QString& txid);

    // 태그로 트랜잭션 ID 검색
    QList<QString> searchTxIdsByTag(const QString& tagQuery);

    // 관찰 주소 구조체
    struct WatchAddress {
        QString address;
        QString label;
        double balance;
        qint64 updatedAt;
    };

    // 관찰 주소 CRUD 및 잔고 관리
    bool addWatchAddress(const QString& address, const QString& label);
    bool removeWatchAddress(const QString& address);
    bool updateWatchAddressBalance(const QString& address, double balance);
    QList<WatchAddress> getWatchAddresses();
    double getWatchAddressesTotalBalance();

private:
    TxAnalytics(QObject* parent = nullptr);
    ~TxAnalytics();

    TxAnalytics(const TxAnalytics&) = delete;
    TxAnalytics& operator=(const TxAnalytics&) = delete;

    static TxAnalytics* m_instance;

    QSqlDatabase m_fileDb;
    bool m_initialized;
};

#endif // XPCHAIN_QT_TXANALYTICS_H
