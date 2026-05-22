#include <qt/txanalytics.h>
#include <QDir>
#include <QDateTime>
#include <QDebug>

TxAnalytics& TxAnalytics::getInstance()
{
    static TxAnalytics instance;
    return instance;
}

TxAnalytics::TxAnalytics(QObject* parent)
    : QObject(parent), m_initialized(false)
{
}

TxAnalytics::~TxAnalytics()
{
    if (m_memDb.isOpen()) {
        m_memDb.close();
    }
    if (m_fileDb.isOpen()) {
        m_fileDb.close();
    }
}

bool TxAnalytics::init(const QString& dataDir)
{
    if (m_initialized) {
        return true;
    }

    // 1. 인메모리 SQLite DB 초기화
    m_memDb = QSqlDatabase::addDatabase("QSQLITE", "memory_db");
    m_memDb.setDatabaseName(":memory:");
    if (!m_memDb.open()) {
        qWarning() << "Failed to open In-Memory Database:" << m_memDb.lastError().text();
        return false;
    }

    QSqlQuery memQuery(m_memDb);
    if (!memQuery.exec("CREATE TABLE IF NOT EXISTS tx_history ("
                        "txid TEXT PRIMARY KEY, "
                        "time INTEGER, "
                        "type INTEGER, "
                        "amount REAL, "
                        "address TEXT)")) {
        qWarning() << "Failed to create tx_history table:" << memQuery.lastError().text();
        return false;
    }

    // 2. 로컬 메타데이터 파일 SQLite DB 초기화
    m_fileDb = QSqlDatabase::addDatabase("QSQLITE", "file_db");
    QString filePath = QDir(dataDir).filePath("tx_metadata.db");
    m_fileDb.setDatabaseName(filePath);
    if (!m_fileDb.open()) {
        qWarning() << "Failed to open File Database:" << m_fileDb.lastError().text();
        return false;
    }

    QSqlQuery fileQuery(m_fileDb);
    if (!fileQuery.exec("CREATE TABLE IF NOT EXISTS tx_tags ("
                         "txid TEXT PRIMARY KEY, "
                         "tag TEXT, "
                         "updated_at INTEGER)")) {
        qWarning() << "Failed to create tx_tags table:" << fileQuery.lastError().text();
        return false;
    }

    m_initialized = true;
    return true;
}

void TxAnalytics::clearHistory()
{
    if (!m_initialized) return;

    QSqlQuery query(m_memDb);
    if (!query.exec("DELETE FROM tx_history")) {
        qWarning() << "Failed to clear tx_history:" << query.lastError().text();
    }
}

bool TxAnalytics::addHistory(const QString& txid, qint64 time, int type, double amount, const QString& address)
{
    if (!m_initialized) return false;

    QSqlQuery query(m_memDb);
    query.prepare("INSERT OR REPLACE INTO tx_history (txid, time, type, amount, address) "
                  "VALUES (:txid, :time, :type, :amount, :address)");
    query.bindValue(":txid", txid);
    query.bindValue(":time", time);
    query.bindValue(":type", type);
    query.bindValue(":amount", amount);
    query.bindValue(":address", address);

    if (!query.exec()) {
        qWarning() << "Failed to add history row:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TxAnalytics::setTag(const QString& txid, const QString& tag)
{
    if (!m_initialized) return false;

    QSqlQuery query(m_fileDb);
    qint64 now = QDateTime::currentSecsSinceEpoch();

    if (tag.trimmed().isEmpty()) {
        // 태그가 비어있으면 삭제
        query.prepare("DELETE FROM tx_tags WHERE txid = :txid");
        query.bindValue(":txid", txid);
    } else {
        // 태그 삽입 또는 교체
        query.prepare("INSERT OR REPLACE INTO tx_tags (txid, tag, updated_at) "
                      "VALUES (:txid, :tag, :updated_at)");
        query.bindValue(":txid", txid);
        query.bindValue(":tag", tag.trimmed());
        query.bindValue(":updated_at", now);
    }

    if (!query.exec()) {
        qWarning() << "Failed to set tag:" << query.lastError().text();
        return false;
    }
    return true;
}

QString TxAnalytics::getTag(const QString& txid)
{
    if (!m_initialized) return "";

    QSqlQuery query(m_fileDb);
    query.prepare("SELECT tag FROM tx_tags WHERE txid = :txid");
    query.bindValue(":txid", txid);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}

QList<QString> TxAnalytics::searchTxIdsByTag(const QString& tagQuery)
{
    QList<QString> results;
    if (!m_initialized || tagQuery.trimmed().isEmpty()) return results;

    QSqlQuery query(m_fileDb);
    query.prepare("SELECT txid FROM tx_tags WHERE tag LIKE :tagQuery");
    query.bindValue(":tagQuery", "%" + tagQuery.trimmed() + "%");

    if (query.exec()) {
        while (query.next()) {
            results.append(query.value(0).toString());
        }
    } else {
        qWarning() << "Failed to search txids by tag:" << query.lastError().text();
    }
    return results;
}

QList<QPair<QString, double>> TxAnalytics::getMonthlyMiningRewards()
{
    QList<QPair<QString, double>> rewards;
    if (!m_initialized) return rewards;

    QSqlQuery query(m_memDb);
    // Generated = 1 (TransactionRecord::Generated)
    if (!query.exec("SELECT strftime('%Y-%m', datetime(time, 'unixepoch', 'localtime')) AS month, "
                    "SUM(amount) AS total "
                    "FROM tx_history "
                    "WHERE type = 1 " // Generated
                    "GROUP BY month "
                    "ORDER BY month ASC")) {
        qWarning() << "Failed to aggregate monthly mining rewards:" << query.lastError().text();
        return rewards;
    }

    while (query.next()) {
        QString month = query.value(0).toString();
        double total = query.value(1).toDouble();
        rewards.append(qMakePair(month, total));
    }
    return rewards;
}
