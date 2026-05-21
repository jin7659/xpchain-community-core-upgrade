// Copyright (c) 2020-2022 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_WALLET_SQLITE_H
#define XPCHAIN_WALLET_SQLITE_H

#include <wallet/db.h>

#ifdef USE_SQLCIPHER
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <fs.h>
#include <support/allocators/secure.h>

/** 파일이 SQLite 데이터베이스인지 확인 */
bool IsSQLiteFile(const fs::path& path);


/** RAII 기반의 SQLite3 데이터베이스 핸들 관리 클래스 */
class SQLiteDatabase : public WalletDatabase
{
private:
    /** SQLite 핸들 */
    sqlite3* m_db{nullptr};

    /** 데이터베이스 파일 경로 */
    const std::string m_path;

    /** 쓰기 가능한지 여부 */
    bool m_writable{false};

    /** SQLCipher 암호화 키 */
    SecureString m_key;

public:
    SQLiteDatabase(const std::string& path, bool writable = true);
    ~SQLiteDatabase();

    /** 암호화 키 설정 */
    void SetKey(const SecureString& key) { m_key = key; }

    /** WalletDatabase 인터페이스 구현 */
    void Open() override;
    void Close() override;
    void Flush() override;
    bool Backup(const std::string& strDest) const override;
    bool Rewrite(const char* pszSkip = nullptr) override;
    bool PeriodicFlush() override;
    void IncrementUpdateCounter() override;
    
    std::string Filename() override { return m_path; }
    std::string Format() override { return "sqlite"; }
    
    /** SQLite 전용 배치 객체 생성 */
    std::unique_ptr<DatabaseBatch> MakeBatch(bool flush_on_close = true) override;
    
    sqlite3* Db() const { return m_db; }

    /** 데이터베이스 파일 검증 */
    static bool Verify(const fs::path& path, std::string& error);

    /** 데이터베이스 파일 복구 (Salvage) */
    static bool Recover(const fs::path& wallet_path, void* callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& newFilename);
};


/** SQLite 배치 처리용 클래스 */
class SQLiteBatch : public DatabaseBatch
{
private:
    SQLiteDatabase& m_database;
    sqlite3_stmt* m_read_stmt{nullptr};
    sqlite3_stmt* m_write_stmt{nullptr};

public:
    explicit SQLiteBatch(SQLiteDatabase& database);
    ~SQLiteBatch();

    /** DatabaseBatch 인터페이스 구현 */
    bool ReadKey(CDataStream&& key, CDataStream& value) override;
    bool WriteKey(CDataStream&& key, CDataStream&& value, bool overwrite = true) override;
    bool EraseKey(CDataStream&& key) override;
    bool HasKey(CDataStream&& key) override;
    
    void Flush() override;
    void Close() override;

    std::unique_ptr<DatabaseCursor> GetCursor() override;
    bool TxnBegin() override;
    bool TxnCommit() override;
    bool TxnAbort() override;
};

/** SQLite 레코드 탐색을 위한 커서 클래스 */
class SQLiteCursor : public DatabaseCursor
{
private:
    SQLiteDatabase& m_database;
    sqlite3_stmt* m_stmt{nullptr};

public:
    explicit SQLiteCursor(SQLiteDatabase& database);
    ~SQLiteCursor();

    int Read(CDataStream& ssKey, CDataStream& ssValue) override;
};

#endif // XPCHAIN_WALLET_SQLITE_H
