// Copyright (c) 2020-2022 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/sqlite.h>

#include <logging.h>
#include <utilstrencodings.h>
#include <streams.h>
#include <fstream>
#include <cstring>
#include <support/cleanse.h>
#include <sys/stat.h>

namespace {

std::string SqlQuote(const std::string& value)
{
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') {
            quoted += "''";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

bool IsPlaintextSQLiteHeader(const fs::path& path)
{
    if (!fs::exists(path) || fs::is_directory(path)) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    char header[16] = {};
    file.read(header, sizeof(header));
    return std::string(header, 15) == "SQLite format 3";
}

bool ExecSql(sqlite3* db, const std::string& sql)
{
    char* errmsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase: SQL failed (%s): %s\n", sql, errmsg ? errmsg : sqlite3_errmsg(db));
        sqlite3_free(errmsg);
        return false;
    }
    sqlite3_free(errmsg);
    return true;
}

bool PrepareForSqlcipherRewrite(sqlite3* db)
{
    if (!ExecSql(db, "PRAGMA journal_mode=DELETE;")) {
        return false;
    }
    if (!ExecSql(db, "VACUUM;")) {
        return false;
    }
    return true;
}

void RemoveWalShmFiles(const std::string& db_path)
{
    fs::remove(db_path + "-wal");
    fs::remove(db_path + "-shm");
}

} // namespace

bool IsSqlcipherEncryptedFile(const fs::path& path)
{
    if (!fs::exists(path) || fs::is_directory(path)) {
        return false;
    }
    // Berkeley DB wallets also lack a plaintext SQLite header; do not treat them
    // as SQLCipher-encrypted (that would incorrectly require -walletdbpassphrase).
    if (IsBerkeleyBDBFile(path)) {
        return false;
    }
    // Encrypted SQLCipher databases do not start with "SQLite format 3".
    return !IsPlaintextSQLiteHeader(path);
}

SQLiteDatabase::SQLiteDatabase(const std::string& path, bool writable)
    : m_path(path), m_writable(writable)
{
}

SQLiteDatabase::~SQLiteDatabase()
{
    Close();
    if (!m_key.empty()) {
        memory_cleanse(m_key.data(), m_key.size());
        m_key.clear();
    }
}

void SQLiteDatabase::Open()
{
    if (m_db) return;

    int flags = SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_SHAREDCACHE;
    if (m_writable) {
        flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    } else {
        flags |= SQLITE_OPEN_READONLY;
    }

    if (sqlite3_open_v2(m_path.c_str(), &m_db, flags, nullptr) != SQLITE_OK) {
        throw std::runtime_error("SQLiteDatabase: Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
    }

    // 멀티스레드 환경에서 잠금 대기 타임아웃 설정 (5초)
    sqlite3_busy_timeout(m_db, 5000);

    // SQLCipher 암호화 적용
    if (!m_key.empty()) {
#ifdef USE_SQLCIPHER
        if (sqlite3_key(m_db, m_key.data(), m_key.size()) != SQLITE_OK) {
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error("SQLiteDatabase: Invalid encryption key");
        }
#else
        sqlite3_close(m_db);
        m_db = nullptr;
        throw std::runtime_error("SQLiteDatabase: Database encryption is not supported (SQLCipher is missing)");
#endif
    }

    // 기본 테이블 생성
    if (m_writable) {
        const char* create_table_sql = "CREATE TABLE IF NOT EXISTS main(key BLOB PRIMARY KEY, value BLOB);";
        char* errmsg = nullptr;
        if (sqlite3_exec(m_db, create_table_sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::string msg = errmsg;
            sqlite3_free(errmsg);
            throw std::runtime_error("SQLiteDatabase: Failed to create table: " + msg);
        }
    }

    // 성능 최적화 PRAGMA 설정
    char* errmsg = nullptr;
    // 메모리 맵 I/O 활성화 (읽기 성능 극대화, 256MB 지정)
    if (sqlite3_exec(m_db, "PRAGMA mmap_size=268435456;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase: Failed to set mmap_size: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }

    if (m_writable) {
        // WAL 모드 설정 (동시성 및 쓰기 속도 대폭 향상)
        if (sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
            LogPrintf("SQLiteDatabase: Failed to set journal_mode to WAL: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
        }
        // synchronous를 NORMAL로 설정 (WAL 모드 시 안전하면서 빠른 쓰기 가능)
        if (sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
            LogPrintf("SQLiteDatabase: Failed to set synchronous to NORMAL: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
        }
        // 캐시 크기 설정 (약 2MB 캐시로 잦은 I/O 오버헤드 완화)
        if (sqlite3_exec(m_db, "PRAGMA cache_size=-2000;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
            LogPrintf("SQLiteDatabase: Failed to set cache_size: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
        }
        // WAL 저널 크기 상한 설정 (4MB로 과도한 디스크 팽창 방지)
        if (sqlite3_exec(m_db, "PRAGMA journal_size_limit=4194304;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
            LogPrintf("SQLiteDatabase: Failed to set journal_size_limit: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
        }
        // 증분 진공(Incremental Auto-Vacuum) 활성화 (백그라운드 빈 공간 점진적 압축 기틀)
        if (sqlite3_exec(m_db, "PRAGMA auto_vacuum=INCREMENTAL;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
            LogPrintf("SQLiteDatabase: Failed to set auto_vacuum to INCREMENTAL: %s\n", errmsg ? errmsg : "unknown");
            sqlite3_free(errmsg);
        }
    }

    // 지갑 오픈 시점에 백그라운드 데이터베이스 실시간 무결성 검증 (PRAGMA quick_check)
    char* errmsg_qc = nullptr;
    auto quick_check_callback = [](void* data, int cols, char** values, char** names) -> int {
        if (cols > 0 && values[0] && strcmp(values[0], "ok") == 0) {
            *(bool*)data = true;
        } else {
            *(bool*)data = false;
        }
        return 0;
    };
    bool quick_check_ok = false;
    if (sqlite3_exec(m_db, "PRAGMA quick_check;", quick_check_callback, &quick_check_ok, &errmsg_qc) != SQLITE_OK || !quick_check_ok) {
        LogPrintf("SQLiteDatabase: quick_check failed: %s. Database may be corrupted.\n", errmsg_qc ? errmsg_qc : "unknown or corrupted");
        if (errmsg_qc) sqlite3_free(errmsg_qc);
    } else {
        LogPrintf("SQLiteDatabase: quick_check passed successfully for %s.\n", m_path);
    }

    // 파일 소유자 전용 최소 권한 강제화 (owner read/write only)
    chmod(m_path.c_str(), 0600);
    
    LogPrintf("SQLiteDatabase: Opened wallet file %s\n", m_path);
}

void SQLiteDatabase::Close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

std::unique_ptr<DatabaseBatch> SQLiteDatabase::MakeBatch(bool flush_on_close)
{
    return std::make_unique<SQLiteBatch>(*this);
}

// SQLiteBatch 구현
SQLiteBatch::SQLiteBatch(SQLiteDatabase& database) : m_database(database)
{
    m_database.Open();
    if (m_database.Db()) {
        sqlite3_prepare_v2(m_database.Db(), "SELECT value FROM main WHERE key = ?;", -1, &m_read_stmt, nullptr);
        sqlite3_prepare_v2(m_database.Db(), "INSERT OR REPLACE INTO main(key, value) VALUES(?, ?);", -1, &m_write_stmt, nullptr);
    }
}

SQLiteBatch::~SQLiteBatch()
{
    Close();
}

bool SQLiteBatch::ReadKey(CDataStream&& key, CDataStream& value)
{
    if (!m_read_stmt) return false;
    
    sqlite3_reset(m_read_stmt);
    sqlite3_clear_bindings(m_read_stmt);
    
    sqlite3_bind_blob(m_read_stmt, 1, key.data(), key.size(), SQLITE_STATIC);
    
    bool found = false;
    if (sqlite3_step(m_read_stmt) == SQLITE_ROW) {
        const void* data = sqlite3_column_blob(m_read_stmt, 0);
        int len = sqlite3_column_bytes(m_read_stmt, 0);
        value.write((const char*)data, len);
        found = true;
    }
    
    return found;
}

bool SQLiteBatch::WriteKey(CDataStream&& key, CDataStream&& value, bool overwrite)
{
    if (!m_write_stmt) return false;
    
    sqlite3_reset(m_write_stmt);
    sqlite3_clear_bindings(m_write_stmt);
    
    sqlite3_bind_blob(m_write_stmt, 1, key.data(), key.size(), SQLITE_STATIC);
    sqlite3_bind_blob(m_write_stmt, 2, value.data(), value.size(), SQLITE_STATIC);
    
    bool success = (sqlite3_step(m_write_stmt) == SQLITE_DONE);
    return success;
}

bool SQLiteBatch::EraseKey(CDataStream&& key)
{
    const char* sql = "DELETE FROM main WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(m_database.Db(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    
    sqlite3_bind_blob(stmt, 1, key.data(), key.size(), SQLITE_STATIC);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool SQLiteBatch::HasKey(CDataStream&& key)
{
    if (!m_database.Db()) return false;
    const char* sql = "SELECT 1 FROM main WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(m_database.Db(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    
    sqlite3_bind_blob(stmt, 1, key.data(), key.size(), SQLITE_STATIC);
    
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

void SQLiteBatch::Flush() {}
void SQLiteBatch::Close()
{
    if (m_read_stmt) {
        sqlite3_finalize(m_read_stmt);
        m_read_stmt = nullptr;
    }
    if (m_write_stmt) {
        sqlite3_finalize(m_write_stmt);
        m_write_stmt = nullptr;
    }
}

std::unique_ptr<DatabaseCursor> SQLiteBatch::GetCursor()
{
    return std::make_unique<SQLiteCursor>(m_database);
}

bool SQLiteBatch::TxnBegin()
{
    return sqlite3_exec(m_database.Db(), "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SQLiteBatch::TxnCommit()
{
    return sqlite3_exec(m_database.Db(), "COMMIT TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SQLiteBatch::TxnAbort()
{
    return sqlite3_exec(m_database.Db(), "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

// SQLiteCursor 구현
SQLiteCursor::SQLiteCursor(SQLiteDatabase& database) : m_database(database), m_stmt(nullptr)
{
}

SQLiteCursor::~SQLiteCursor()
{
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
    }
}

int SQLiteCursor::Read(CDataStream& key, CDataStream& value)
{
    // Lazy prepare: non-empty key => seek (>= key), empty key => full ordered scan.
    if (!m_stmt) {
        if (!key.empty()) {
            const char* sql = "SELECT key, value FROM main WHERE key >= ?1 ORDER BY key;";
            if (sqlite3_prepare_v2(m_database.Db(), sql, -1, &m_stmt, nullptr) != SQLITE_OK) {
                m_stmt = nullptr;
                return -1;
            }
            if (sqlite3_bind_blob(m_stmt, 1, key.data(), key.size(), SQLITE_TRANSIENT) != SQLITE_OK) {
                sqlite3_finalize(m_stmt);
                m_stmt = nullptr;
                return -1;
            }
        } else {
            const char* sql = "SELECT key, value FROM main ORDER BY key;";
            if (sqlite3_prepare_v2(m_database.Db(), sql, -1, &m_stmt, nullptr) != SQLITE_OK) {
                m_stmt = nullptr;
                return -1;
            }
        }
    }

    int res = sqlite3_step(m_stmt);
    if (res == SQLITE_ROW) {
        key.clear();
        value.clear();

        const void* key_data = sqlite3_column_blob(m_stmt, 0);
        int key_len = sqlite3_column_bytes(m_stmt, 0);
        if (key_data && key_len > 0) {
            key.write((const char*)key_data, key_len);
        }

        const void* val_data = sqlite3_column_blob(m_stmt, 1);
        int val_len = sqlite3_column_bytes(m_stmt, 1);
        if (val_data && val_len > 0) {
            value.write((const char*)val_data, val_len);
        }

        return 0; // Success
    } else if (res == SQLITE_DONE) {
        return DB_NOTFOUND;
    }

    return -1; // Error
}

// 나머지 필수 구현들
void SQLiteDatabase::Flush()
{
    if (!m_db) return;

    // WAL 모드의 미결합 로그 페이지를 메인 파일로 안전하게 결합 및 체크포인트 수행
    int nLog = 0, nCkpt = 0;
    int rc = sqlite3_wal_checkpoint_v2(m_db, nullptr, SQLITE_CHECKPOINT_PASSIVE, &nLog, &nCkpt);
    if (rc != SQLITE_OK) {
        LogPrintf("SQLiteDatabase: WAL checkpoint failed: %s\n", sqlite3_errmsg(m_db));
    } else {
        LogPrint(BCLog::DB, "SQLiteDatabase: WAL checkpoint successful. %d log pages, %d checkpointed\n", nLog, nCkpt);
    }
}

bool SQLiteDatabase::Backup(const std::string& strDest) const
{
    if (!m_db) return false;

    fs::path pathSrc(m_path);
    fs::path pathDest(strDest);
    if (fs::is_directory(pathDest)) {
        pathDest /= pathSrc.filename();
    }

    try {
        if (fs::exists(pathDest) && fs::equivalent(pathSrc, pathDest)) {
            LogPrintf("SQLiteDatabase: cannot backup to wallet source file %s\n", pathDest.string());
            return false;
        }
    } catch (const fs::filesystem_error& e) {
        LogPrintf("SQLiteDatabase: Backup path check failed: %s\n", e.what());
        return false;
    }

    const std::string dest_path = pathDest.string();
    sqlite3* pDestDb = nullptr;
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(dest_path.c_str(), &pDestDb, flags, nullptr) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase: Backup failed - Unable to open destination database: %s\n", sqlite3_errmsg(pDestDb));
        if (pDestDb) sqlite3_close(pDestDb);
        return false;
    }

    // 암호화된 원본 지갑 파일인 경우, 백업 대상 파일도 안전하게 동일 키로 암호화 설정
    if (!m_key.empty()) {
#ifdef USE_SQLCIPHER
        if (sqlite3_key(pDestDb, m_key.data(), m_key.size()) != SQLITE_OK) {
            LogPrintf("SQLiteDatabase: Backup failed - Unable to encrypt destination database: %s\n", sqlite3_errmsg(pDestDb));
            sqlite3_close(pDestDb);
            return false;
        }
#else
        LogPrintf("SQLiteDatabase: Backup failed - SQLCipher key specified but SQLCipher is not enabled\n");
        sqlite3_close(pDestDb);
        return false;
#endif
    }

    // 온라인 백업 세션 초기화
    sqlite3_backup* pBackup = sqlite3_backup_init(pDestDb, "main", m_db, "main");
    if (!pBackup) {
        LogPrintf("SQLiteDatabase: Backup failed - Unable to initialize backup: %s\n", sqlite3_errmsg(pDestDb));
        sqlite3_close(pDestDb);
        return false;
    }

    // 10페이지씩 쪼개어 복사 수행 (디스크 I/O를 타 스레드에 양보하여 메인 멈춤 현상 원천 방어)
    int rc = SQLITE_OK;
    do {
        rc = sqlite3_backup_step(pBackup, 10);
        if (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            // 일시적인 락이나 동시 작업 경합 시 10ms 대기하여 양보합니다.
            MilliSleep(10);
        }
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

    sqlite3_backup_finish(pBackup);

    if (rc != SQLITE_DONE) {
        LogPrintf("SQLiteDatabase: Backup failed during execution: %s (code %d)\n", sqlite3_errmsg(pDestDb), rc);
        sqlite3_close(pDestDb);
        return false;
    }

    sqlite3_close(pDestDb);
    
    // 백업 생성 파일 권한을 소유자 전용 최소 권한(0600)으로 제한
    chmod(dest_path.c_str(), 0600);

    LogPrintf("SQLiteDatabase: Successfully backed up wallet database to %s\n", dest_path);
    return true;
}
bool SQLiteDatabase::Rewrite(const char* pszSkip)
{
    if (!m_db) return false;

#ifdef USE_SQLCIPHER
    if (!m_key.empty()) {
        if (!PrepareForSqlcipherRewrite(m_db)) {
            return false;
        }

        const bool plaintext = IsPlaintextSQLiteHeader(m_path);
        if (plaintext) {
            const std::string temp_path = m_path + ".encrypt.tmp";
            const std::string passphrase(m_key.begin(), m_key.end());
            RemoveWalShmFiles(temp_path);
            fs::remove(temp_path);

            const std::string attach_sql = strprintf(
                "ATTACH DATABASE %s AS encrypted KEY %s;",
                SqlQuote(temp_path).c_str(),
                SqlQuote(passphrase).c_str());
            if (!ExecSql(m_db, attach_sql)) {
                return false;
            }

            bool export_ok = ExecSql(m_db, "SELECT sqlcipher_export('encrypted');");
            ExecSql(m_db, "DETACH DATABASE encrypted;");
            if (!export_ok) {
                fs::remove(temp_path);
                return false;
            }

            Close();
            RemoveWalShmFiles(m_path);

            const std::string backup_path = m_path + ".plain.bak";
            try {
                fs::remove(backup_path);
                fs::rename(m_path, backup_path);
                fs::rename(temp_path, m_path);
                fs::remove(backup_path);
            } catch (const fs::filesystem_error& e) {
                LogPrintf("SQLiteDatabase: Failed to replace wallet file during encryption: %s\n", e.what());
                return false;
            }

            RemoveWalShmFiles(m_path);
            Open();
            LogPrintf("SQLiteDatabase: Encrypted plaintext database %s with SQLCipher\n", m_path);
            return true;
        }

        if (sqlite3_rekey(m_db, m_key.data(), m_key.size()) != SQLITE_OK) {
            LogPrintf("SQLiteDatabase: Failed to rekey database: %s\n", sqlite3_errmsg(m_db));
            return false;
        }

        RemoveWalShmFiles(m_path);
        if (!ExecSql(m_db, "PRAGMA journal_mode=WAL;")) {
            LogPrintf("SQLiteDatabase: Failed to restore WAL journal mode after rekey\n");
        }
        LogPrintf("SQLiteDatabase: Database rekey successful for %s\n", m_path);
        return true;
    }
#endif

    // VACUUM을 통해 보안상 남아있을 수 있는 평문 데이터를 완전히 제거하고 파일을 정리합니다.
    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, "VACUUM;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase: VACUUM failed: %s\n", errmsg);
        sqlite3_free(errmsg);
        return false;
    }

    return true;
}
bool SQLiteDatabase::PeriodicFlush()
{
    if (!m_db) return false;
    
    // 주기적 플러시 요청 시, 백그라운드 스레드에서 WAL 데이터를 메인 파일에 정밀 병합합니다.
    int nLog = 0, nCkpt = 0;
    int rc = sqlite3_wal_checkpoint_v2(m_db, nullptr, SQLITE_CHECKPOINT_PASSIVE, &nLog, &nCkpt);
    if (rc != SQLITE_OK) {
        LogPrintf("SQLiteDatabase::PeriodicFlush: WAL checkpoint failed: %s\n", sqlite3_errmsg(m_db));
        return false;
    }

    // 증분 진공(Incremental Auto-Vacuum)을 통해 50페이지 단위로 빈 물리 용량을 무부하 자동 회수
    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, "PRAGMA incremental_vacuum(50);", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase::PeriodicFlush: incremental_vacuum failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }

    return true;
}

void SQLiteDatabase::IncrementUpdateCounter()
{
    ++nUpdateCounter;
}

bool IsSQLiteFile(const fs::path& path)
{
    if (!fs::exists(path)) {
        return false;
    }
    if (fs::is_directory(path)) {
        return false;
    }
    
    // 명백한 Berkeley DB 파일이 아닌 경우 SQLite(SQLCipher 암호화 지갑 포함)로 처리합니다.
    return !IsBerkeleyBDBFile(path);
}

bool SQLiteDatabase::Verify(const fs::path& path, std::string& error)
{
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        error = "SQLiteDatabase: Failed to open for verification";
        if (db) sqlite3_close(db);
        return false;
    }

    char* errmsg = nullptr;
    bool ok = true;
    auto callback = [](void* data, int cols, char** values, char** names) -> int {
        if (cols > 0 && values[0] && strcmp(values[0], "ok") == 0) {
            *(bool*)data = true;
        } else {
            *(bool*)data = false;
        }
        return 0;
    };

    bool integrity_ok = false;
    int rc = sqlite3_exec(db, "PRAGMA integrity_check;", callback, &integrity_ok, &errmsg);
    if (rc != SQLITE_OK) {
        if (rc == SQLITE_NOTADB) {
            // SQLCipher 암호화 지갑인 경우, 키 입력 전에는 SQLITE_NOTADB(26) 오류가 발생합니다.
            // 이는 지갑이 안전하게 암호화되어 있음을 뜻하므로 검증 성공으로 처리하여 복호화 단계로 진행되도록 유도합니다.
            LogPrintf("SQLiteDatabase::Verify: Database %s appears to be encrypted (SQLCipher). Postponing integrity check.\n", path.string());
            sqlite3_free(errmsg);
            ok = true;
        } else {
            error = errmsg ? errmsg : "Integrity check failed";
            sqlite3_free(errmsg);
            ok = false;
        }
    } else {
        ok = integrity_ok;
        if (!ok) error = "SQLite integrity check failed";
    }

    sqlite3_close(db);
    return ok;
}

bool SQLiteDatabase::Recover(const fs::path& wallet_path, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& newFilename)
{
    std::string filename = wallet_path.filename().string();
    int64_t now = GetTime();
    newFilename = strprintf("%s.%d.bak", filename, now);
    
    fs::path backup_path = wallet_path.parent_path() / newFilename;
    
    // 파일 백업 이름으로 이동
    try {
        fs::rename(wallet_path, backup_path);
        // 격리된 임시 백업 파일도 소유자 전용 권한(0600)으로 제한
        chmod(backup_path.string().c_str(), 0600);
        LogPrintf("SQLiteDatabase::Recover: Renamed %s to %s\n", wallet_path.string(), backup_path.string());
    } catch (const fs::filesystem_error& e) {
        LogPrintf("SQLiteDatabase::Recover: Failed to rename %s to %s: %s\n", wallet_path.string(), backup_path.string(), e.what());
        return false;
    }
    
    // 복구 작업 수행
    // 1. 백업 데이터베이스 오픈 (읽기 전용)
    sqlite3* pSrcDb = nullptr;
    if (sqlite3_open_v2(backup_path.string().c_str(), &pSrcDb, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase::Recover: Failed to open backup database: %s\n", sqlite3_errmsg(pSrcDb));
        if (pSrcDb) sqlite3_close(pSrcDb);
        return false;
    }
    
    // 2. 새 지갑 데이터베이스 파일 생성
    sqlite3* pDestDb = nullptr;
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(wallet_path.string().c_str(), &pDestDb, flags, nullptr) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase::Recover: Failed to create fresh database: %s\n", sqlite3_errmsg(pDestDb));
        sqlite3_close(pSrcDb);
        if (pDestDb) sqlite3_close(pDestDb);
        return false;
    }
    
    // 대상 디비에 테이블 생성
    const char* create_table_sql = "CREATE TABLE IF NOT EXISTS main(key BLOB PRIMARY KEY, value BLOB);";
    char* errmsg = nullptr;
    if (sqlite3_exec(pDestDb, create_table_sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase::Recover: Failed to create table in new database: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_close(pSrcDb);
        sqlite3_close(pDestDb);
        return false;
    }
    
    // 3. 백업 디비로부터 데이터 조회
    const char* select_sql = "SELECT key, value FROM main;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pSrcDb, select_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogPrintf("SQLiteDatabase::Recover: Failed to prepare select statement on backup database: %s\n", sqlite3_errmsg(pSrcDb));
        sqlite3_close(pSrcDb);
        sqlite3_close(pDestDb);
        return false;
    }
    
    // 4. 레코드 복제
    bool fSuccess = true;
    int salvaged_count = 0;
    
    // 트랜잭션 시작
    sqlite3_exec(pDestDb, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    sqlite3_stmt* write_stmt = nullptr;
    sqlite3_prepare_v2(pDestDb, "INSERT OR REPLACE INTO main(key, value) VALUES(?, ?);", -1, &write_stmt, nullptr);
    
    if (!write_stmt) {
        LogPrintf("SQLiteDatabase::Recover: Failed to prepare insert statement in new database\n");
        sqlite3_finalize(stmt);
        sqlite3_close(pSrcDb);
        sqlite3_close(pDestDb);
        return false;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* key_data = sqlite3_column_blob(stmt, 0);
        int key_len = sqlite3_column_bytes(stmt, 0);
        const void* val_data = sqlite3_column_blob(stmt, 1);
        int val_len = sqlite3_column_bytes(stmt, 1);
        
        if (!key_data || key_len <= 0) continue;
        
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write((const char*)key_data, key_len);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        if (val_data && val_len > 0) {
            ssValue.write((const char*)val_data, val_len);
        }
        
        // 필터 콜백이 있으면 검사
        if (recoverKVcallback) {
            if (!(*recoverKVcallback)(callbackDataIn, ssKey, ssValue)) {
                continue;
            }
        }
        
        sqlite3_reset(write_stmt);
        sqlite3_clear_bindings(write_stmt);
        sqlite3_bind_blob(write_stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC);
        sqlite3_bind_blob(write_stmt, 2, ssValue.data(), ssValue.size(), SQLITE_STATIC);
        
        if (sqlite3_step(write_stmt) != SQLITE_DONE) {
            LogPrintf("SQLiteDatabase::Recover: Failed to insert salvaged record\n");
            fSuccess = false;
            break;
        }
        salvaged_count++;
    }
    
    sqlite3_finalize(stmt);
    sqlite3_finalize(write_stmt);
    
    if (fSuccess) {
        sqlite3_exec(pDestDb, "COMMIT TRANSACTION;", nullptr, nullptr, nullptr);
        LogPrintf("SQLiteDatabase::Recover: Successfully salvaged %d records from %s\n", salvaged_count, newFilename);
    } else {
        sqlite3_exec(pDestDb, "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr);
    }
    
    sqlite3_close(pSrcDb);
    sqlite3_close(pDestDb);
    
    // 새로 복구 작성된 지갑 데이터베이스 파일 권한을 소유자 전용 최소 권한(0600)으로 제한
    chmod(wallet_path.string().c_str(), 0600);
    
    return fSuccess && (salvaged_count > 0);
}

