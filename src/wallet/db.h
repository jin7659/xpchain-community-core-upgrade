// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_WALLET_DB_H
#define XPCHAIN_WALLET_DB_H

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <streams.h>
#include <sync.h>
#include <util.h>
#include <version.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <db_cxx.h>

static const unsigned int DEFAULT_WALLET_DBLOGSIZE = 100;
static const bool DEFAULT_WALLET_PRIVDB = true;

/** 데이터베이스 레코드 탐색을 위한 추상 커서 */
class DatabaseCursor
{
public:
    virtual ~DatabaseCursor() {}
    virtual int Read(CDataStream& ssKey, CDataStream& ssValue) = 0;
};

/** 지갑 데이터를 읽고 쓰기 위한 트랜잭션/배치 추상 인터페이스 */
class DatabaseBatch
{
public:
    virtual ~DatabaseBatch() {}

    virtual void Flush() = 0;
    virtual void Close() = 0;

    virtual bool ReadKey(CDataStream&& key, CDataStream& value) = 0;
    virtual bool WriteKey(CDataStream&& key, CDataStream&& value, bool overwrite = true) = 0;
    virtual bool EraseKey(CDataStream&& key) = 0;
    virtual bool HasKey(CDataStream&& key) = 0;

    /** 커서 생성 및 데이터 탐색 */
    virtual std::unique_ptr<DatabaseCursor> GetCursor() = 0;

    /** 트랜잭션 관리 */
    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual bool TxnAbort() = 0;
};

/** 지갑 데이터베이스 엔진(BDB, SQLite)을 위한 추상 인터페이스 */
class WalletDatabase
{
public:
    virtual ~WalletDatabase() {}

    virtual void Open() = 0;
    virtual void Close() = 0;
    virtual void Flush() = 0;
    virtual bool Backup(const std::string& strDest) const = 0;
    virtual bool Rewrite(const char* pszSkip = nullptr) = 0;
    virtual bool PeriodicFlush() = 0;
    virtual void IncrementUpdateCounter() = 0;

    virtual std::string Filename() = 0;
    virtual std::string Format() = 0;
    virtual std::unique_ptr<DatabaseBatch> MakeBatch(bool flush_on_close = true) = 0;
    
    std::atomic<unsigned int> nUpdateCounter{0};
    unsigned int nLastSeen{0};
    unsigned int nLastFlushed{0};
    int64_t nLastWalletUpdate{0};
};

class BerkeleyEnvironment
{
private:
    bool fDbEnvInit;
    bool fMockDb;
    // Don't change into fs::path, as that can result in
    // shutdown problems/crashes caused by a static initialized internal pointer.
    std::string strPath;

public:
    std::unique_ptr<DbEnv> dbenv;
    std::map<std::string, int> mapFileUseCount;
    std::map<std::string, Db*> mapDb;

    BerkeleyEnvironment(const fs::path& env_directory);
    ~BerkeleyEnvironment();
    void Reset();

    void MakeMock();
    bool IsMock() const { return fMockDb; }
    bool IsInitialized() const { return fDbEnvInit; }
    fs::path Directory() const { return strPath; }

    /**
     * Verify that database file strFile is OK. If it is not,
     * call the callback to try to recover.
     * This must be called BEFORE strFile is opened.
     * Returns true if strFile is OK.
     */
    enum class VerifyResult { VERIFY_OK,
                        RECOVER_OK,
                        RECOVER_FAIL };
    typedef bool (*recoverFunc_type)(const fs::path& file_path, std::string& out_backup_filename);
    VerifyResult Verify(const std::string& strFile, recoverFunc_type recoverFunc, std::string& out_backup_filename);
    /**
     * Salvage data from a file that Verify says is bad.
     * fAggressive sets the DB_AGGRESSIVE flag (see berkeley DB->verify() method documentation).
     * Appends binary key/value pairs to vResult, returns true if successful.
     * NOTE: reads the entire database into memory, so cannot be used
     * for huge databases.
     */
    typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char> > KeyValPair;
    bool Salvage(const std::string& strFile, bool fAggressive, std::vector<KeyValPair>& vResult);

    bool Open(bool retry);
    void Close();
    void Flush(bool fShutdown);
    void CheckpointLSN(const std::string& strFile);

    void CloseDb(const std::string& strFile);

    DbTxn* TxnBegin(int flags = DB_TXN_WRITE_NOSYNC)
    {
        DbTxn* ptxn = nullptr;
        int ret = dbenv->txn_begin(nullptr, &ptxn, flags);
        if (!ptxn || ret != 0)
            return nullptr;
        return ptxn;
    }
};

/** Get BerkeleyEnvironment and database filename given a wallet path. */
BerkeleyEnvironment* GetWalletEnv(const fs::path& wallet_path, std::string& database_filename);

/** An instance of this class represents one database.
 * For BerkeleyDB this is just a (env, strFile) tuple.
 **/
class BerkeleyDatabase : public WalletDatabase
{
    friend class BerkeleyBatch;
public:
    /** Create dummy DB handle */
    BerkeleyDatabase() : env(nullptr)
    {
    }

    /** Create DB handle to real database */
    BerkeleyDatabase(const fs::path& wallet_path, bool mock = false)
    {
        env = GetWalletEnv(wallet_path, strFile);
        if (mock) {
            env->Close();
            env->Reset();
            env->MakeMock();
        }
    }

    /** Return object for accessing database at specified path. */
    static std::unique_ptr<BerkeleyDatabase> Create(const fs::path& path)
    {
        return MakeUnique<BerkeleyDatabase>(path);
    }

    /** Return object for accessing dummy database with no read/write capabilities. */
    static std::unique_ptr<BerkeleyDatabase> CreateDummy()
    {
        return MakeUnique<BerkeleyDatabase>();
    }

    /** Return object for accessing temporary in-memory database. */
    static std::unique_ptr<BerkeleyDatabase> CreateMock()
    {
        return MakeUnique<BerkeleyDatabase>("", true /* mock */);
    }

    void Open() override;
    void Close() override;
    void Flush() override;
    bool Backup(const std::string& strDest) const override;
    bool Rewrite(const char* pszSkip = nullptr) override;
    bool PeriodicFlush() override;
    void IncrementUpdateCounter() override;

    std::string Filename() override { return strFile; }
    std::string Format() override { return "berkeley"; }
    std::unique_ptr<DatabaseBatch> MakeBatch(bool flush_on_close = true) override;


private:
    /** BerkeleyDB specific */
    BerkeleyEnvironment *env;
    std::string strFile;

    /** Return whether this database handle is a dummy for testing.
     * Only to be used at a low level, application should ideally not care
     * about this.
     */
    bool IsDummy() const { return env == nullptr; }
};


/** RAII class that provides access to a Berkeley database */
class BerkeleyBatch : public DatabaseBatch
{
protected:
    Db* pdb;
    std::string strFile;
    DbTxn* activeTxn;
    bool fReadOnly;
    bool fFlushOnClose;
    BerkeleyEnvironment *env;

public:
    explicit BerkeleyBatch(BerkeleyDatabase& database, const char* pszMode = "r+", bool fFlushOnCloseIn=true);
    ~BerkeleyBatch() { Close(); }

    BerkeleyBatch(const BerkeleyBatch&) = delete;
    BerkeleyBatch& operator=(const BerkeleyBatch&) = delete;

    void Flush() override;
    void Close() override;

    bool ReadKey(CDataStream&& key, CDataStream& value) override;
    bool WriteKey(CDataStream&& key, CDataStream&& value, bool overwrite = true) override;
    bool EraseKey(CDataStream&& key) override;
    bool HasKey(CDataStream&& key) override;
    std::unique_ptr<DatabaseCursor> GetCursor() override;

    static bool Recover(const fs::path& file_path, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename);

    /* flush the wallet passively (TRY_LOCK)
       ideal to be called periodically */
    static bool PeriodicFlush(BerkeleyDatabase& database);
    /* verifies the database environment */
    static bool VerifyEnvironment(const fs::path& file_path, std::string& errorStr);
    /* verifies the database file */
    static bool VerifyDatabaseFile(const fs::path& file_path, std::string& warningStr, std::string& errorStr, BerkeleyEnvironment::recoverFunc_type recoverFunc);


public:
    bool TxnBegin() override
    {
        if (!pdb || activeTxn)
            return false;
        DbTxn* ptxn = env->TxnBegin();
        if (!ptxn)
            return false;
        activeTxn = ptxn;
        return true;
    }

    bool TxnCommit() override
    {
        if (!pdb || !activeTxn)
            return false;
        int ret = activeTxn->commit(0);
        activeTxn = nullptr;
        return (ret == 0);
    }

    bool TxnAbort() override
    {
        if (!pdb || !activeTxn)
            return false;
        int ret = activeTxn->abort();
        activeTxn = nullptr;
        return (ret == 0);
    }

    bool static Rewrite(BerkeleyDatabase& database, const char* pszSkip = nullptr);
};

/** 지갑 경로와 플래그에 따라 적절한 데이터베이스 엔진을 생성하는 팩토리 함수 */
std::unique_ptr<WalletDatabase> CreateWalletDatabase(const fs::path& path, uint64_t wallet_creation_flags = 0);

/** BerkeleyDB 커서 구현체 */
class BerkeleyCursor : public DatabaseCursor
{
private:
    Dbc* m_cursor;
public:
    BerkeleyCursor(Dbc* cursor) : m_cursor(cursor) {}
    ~BerkeleyCursor() { if (m_cursor) m_cursor->close(); }
    int Read(CDataStream& ssKey, CDataStream& ssValue) override;
};

#endif // XPCHAIN_WALLET_DB_H
