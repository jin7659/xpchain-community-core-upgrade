// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/sqlite.h>

#include <logging.h>
#include <utilstrencodings.h>
#include <streams.h>
#include <fstream>
#include <cstring>


SQLiteDatabase::SQLiteDatabase(const std::string& path, bool writable)
    : m_path(path), m_writable(writable)
{
}

SQLiteDatabase::~SQLiteDatabase()
{
    Close();
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
}

SQLiteBatch::~SQLiteBatch()
{
    Close();
}

bool SQLiteBatch::ReadKey(CDataStream&& key, CDataStream& value)
{
    const char* sql = "SELECT value FROM main WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(m_database.Db(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    
    sqlite3_bind_blob(stmt, 1, key.data(), key.size(), SQLITE_STATIC);
    
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* data = sqlite3_column_blob(stmt, 0);
        int len = sqlite3_column_bytes(stmt, 0);
        value.write((const char*)data, len);
        found = true;
    }
    
    sqlite3_finalize(stmt);
    return found;
}

bool SQLiteBatch::WriteKey(CDataStream&& key, CDataStream&& value, bool overwrite)
{
    const char* sql = "INSERT OR REPLACE INTO main(key, value) VALUES(?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(m_database.Db(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    
    sqlite3_bind_blob(stmt, 1, key.data(), key.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, value.data(), value.size(), SQLITE_STATIC);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
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
void SQLiteBatch::Close() {}

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
SQLiteCursor::SQLiteCursor(SQLiteDatabase& database) : m_database(database)
{
    const char* sql = "SELECT key, value FROM main;";
    if (sqlite3_prepare_v2(m_database.Db(), sql, -1, &m_stmt, nullptr) != SQLITE_OK) {
        m_stmt = nullptr;
    }
}

SQLiteCursor::~SQLiteCursor()
{
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
    }
}

int SQLiteCursor::Read(CDataStream& key, CDataStream& value)
{
    if (!m_stmt) return -1;

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

// 나머지 필수 구현들 (Stub)
void SQLiteDatabase::Flush() {}
bool SQLiteDatabase::Backup(const std::string& strDest) const { return false; }
bool SQLiteDatabase::Rewrite(const char* pszSkip) { return true; }
bool SQLiteDatabase::PeriodicFlush() { return true; }
void SQLiteDatabase::IncrementUpdateCounter() {}

bool IsSQLiteFile(const fs::path& path)
{
    if (!fs::exists(path)) {
        return false;
    }
    if (fs::is_directory(path)) {
        return false;
    }
    std::ifstream file(path.string(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    char magic[16];
    file.read(magic, 16);
    if (file.gcount() == 16 && memcmp(magic, "SQLite format 3\0", 16) == 0) {
        return true;
    }
    return false;
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
    if (sqlite3_exec(db, "PRAGMA integrity_check;", callback, &integrity_ok, &errmsg) != SQLITE_OK) {
        error = errmsg ? errmsg : "Integrity check failed";
        sqlite3_free(errmsg);
        ok = false;
    } else {
        ok = integrity_ok;
        if (!ok) error = "SQLite integrity check failed";
    }

    sqlite3_close(db);
    return ok;
}

