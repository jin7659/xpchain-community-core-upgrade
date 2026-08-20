#include <qt/txanalytics.h>
#include <QDir>
#include <QDateTime>
#include <QDebug>

TxAnalytics* TxAnalytics::m_instance = nullptr;

TxAnalytics& TxAnalytics::getInstance()
{
    if (!m_instance) {
        m_instance = new TxAnalytics();
    }
    return *m_instance;
}

void TxAnalytics::destroy()
{
    if (m_instance) {
        delete m_instance;
    }
    m_instance = nullptr;
}

TxAnalytics::TxAnalytics(QObject* parent)
    : QObject(parent), m_initialized(false)
{
}

TxAnalytics::~TxAnalytics()
{
    if (m_fileDb.isOpen()) {
        m_fileDb.close();
    }

    m_fileDb = QSqlDatabase();
    QSqlDatabase::removeDatabase("file_db");
}

bool TxAnalytics::init(const QString& dataDir)
{
    if (m_initialized) {
        return true;
    }

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

    if (!fileQuery.exec("CREATE TABLE IF NOT EXISTS watch_addresses ("
                         "address TEXT PRIMARY KEY, "
                         "label TEXT, "
                         "balance REAL, "
                         "updated_at INTEGER)")) {
        qWarning() << "Failed to create watch_addresses table:" << fileQuery.lastError().text();
        return false;
    }

    m_initialized = true;
    return true;
}

bool TxAnalytics::setTag(const QString& txid, const QString& tag)
{
    if (!m_initialized) return false;

    QSqlQuery query(m_fileDb);
    qint64 now = QDateTime::currentSecsSinceEpoch();

    if (tag.trimmed().isEmpty()) {
        query.prepare("DELETE FROM tx_tags WHERE txid = :txid");
        query.bindValue(":txid", txid);
    } else {
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

bool TxAnalytics::addWatchAddress(const QString& address, const QString& label)
{
    if (!m_initialized || address.trimmed().isEmpty()) return false;

    QSqlQuery query(m_fileDb);
    qint64 now = QDateTime::currentSecsSinceEpoch();
    query.prepare("INSERT OR REPLACE INTO watch_addresses (address, label, balance, updated_at) "
                  "VALUES (:address, :label, 0.0, :updated_at)");
    query.bindValue(":address", address.trimmed());
    query.bindValue(":label", label.trimmed());
    query.bindValue(":updated_at", now);

    if (!query.exec()) {
        qWarning() << "Failed to add watch address:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TxAnalytics::removeWatchAddress(const QString& address)
{
    if (!m_initialized) return false;

    QSqlQuery query(m_fileDb);
    query.prepare("DELETE FROM watch_addresses WHERE address = :address");
    query.bindValue(":address", address.trimmed());

    if (!query.exec()) {
        qWarning() << "Failed to remove watch address:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TxAnalytics::updateWatchAddressBalance(const QString& address, double balance)
{
    if (!m_initialized) return false;

    QSqlQuery query(m_fileDb);
    qint64 now = QDateTime::currentSecsSinceEpoch();
    query.prepare("UPDATE watch_addresses SET balance = :balance, updated_at = :updated_at "
                  "WHERE address = :address");
    query.bindValue(":balance", balance);
    query.bindValue(":updated_at", now);
    query.bindValue(":address", address.trimmed());

    if (!query.exec()) {
        qWarning() << "Failed to update watch address balance:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<TxAnalytics::WatchAddress> TxAnalytics::getWatchAddresses()
{
    QList<WatchAddress> list;
    if (!m_initialized) return list;

    QSqlQuery query(m_fileDb);
    if (query.exec("SELECT address, label, balance, updated_at FROM watch_addresses ORDER BY address ASC")) {
        while (query.next()) {
            WatchAddress wa;
            wa.address = query.value(0).toString();
            wa.label = query.value(1).toString();
            wa.balance = query.value(2).toDouble();
            wa.updatedAt = query.value(3).toLongLong();
            list.append(wa);
        }
    } else {
        qWarning() << "Failed to get watch addresses:" << query.lastError().text();
    }
    return list;
}

double TxAnalytics::getWatchAddressesTotalBalance()
{
    if (!m_initialized) return 0.0;

    QSqlQuery query(m_fileDb);
    if (query.exec("SELECT SUM(balance) FROM watch_addresses")) {
        if (query.next()) {
            return query.value(0).toDouble();
        }
    } else {
        qWarning() << "Failed to get watch addresses total balance:" << query.lastError().text();
    }
    return 0.0;
}
