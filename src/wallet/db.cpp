#include <config/xpchain-config.h>

#include <wallet/db.h>

#include <addrman.h>
#include <hash.h>
#include <logging.h>
#include <protocol.h>
#include <utilstrencodings.h>
#include <wallet/walletutil.h>

#include <stdint.h>

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <boost/thread.hpp>

namespace {

//! Make sure database has a unique fileid within the environment. If it
//! doesn't, throw an error. BDB caches do not work properly when more than one
//! open database has the same fileid (values written to one database may show
//! up in reads to other databases).
//!
//! BerkeleyDB generates unique fileids by default
//! (https://docs.oracle.com/cd/E17275_01/html/programmer_reference/program_copy.html),
//! so xpchain should never create different databases with the same fileid, but
//! this error can be triggered if users manually copy database files.
void CheckUniqueFileid(const BerkeleyEnvironment& env, const std::string& filename, Db& db)
{
    if (env.IsMock()) return;

    u_int8_t fileid[DB_FILE_ID_LEN];
    int ret = db.get_mpf()->get_fileid(fileid);
    if (ret != 0) {
        throw std::runtime_error(strprintf("BerkeleyBatch: Can't open database %s (get_fileid failed with %d)", filename, ret));
    }

    for (const auto& item : env.mapDb) {
        u_int8_t item_fileid[DB_FILE_ID_LEN];
        if (item.second && item.second->get_mpf()->get_fileid(item_fileid) == 0 &&
            memcmp(fileid, item_fileid, sizeof(fileid)) == 0) {
            const char* item_filename = nullptr;
            item.second->get_dbname(&item_filename, nullptr);
            throw std::runtime_error(strprintf("BerkeleyBatch: Can't open database %s (duplicates fileid %s from %s)", filename,
                HexStr(std::begin(item_fileid), std::end(item_fileid)),
                item_filename ? item_filename : "(unknown database)"));
        }
    }
}

CCriticalSection cs_db;
std::map<std::string, BerkeleyEnvironment> g_dbenvs GUARDED_BY(cs_db); //!< Map from directory name to open db environment.
} // namespace

BerkeleyEnvironment* GetWalletEnv(const fs::path& wallet_path, std::string& database_filename)
{
    fs::path env_directory;
    if (fs::is_regular_file(wallet_path)) {
        // Special case for backwards compatibility: if wallet path points to an
        // existing file, treat it as the path to a BDB data file in a parent
        // directory that also contains BDB log files.
        env_directory = wallet_path.parent_path();
        database_filename = wallet_path.filename().string();
    } else {
        // Normal case: Interpret wallet path as a directory path containing
        // data and log files.
        env_directory = wallet_path;
        database_filename = "wallet.dat";
    }
    LOCK(cs_db);
    // Note: An ununsed temporary BerkeleyEnvironment object may be created inside the
    // emplace function if the key already exists. This is a little inefficient,
    // but not a big concern since the map will be changed in the future to hold
    // pointers instead of objects, anyway.
    return &g_dbenvs.emplace(std::piecewise_construct, std::forward_as_tuple(env_directory.string()), std::forward_as_tuple(env_directory)).first->second;
}

void BerkeleyEnvironment::MakeMock()
{
    if (fDbEnvInit)
        throw std::runtime_error("BerkeleyEnvironment::MakeMock: Already initialized");

    boost::this_thread::interruption_point();

    LogPrint(BCLog::DB, "BerkeleyEnvironment::MakeMock\n");

    dbenv->set_cachesize(1, 0, 1);
    dbenv->set_lg_bsize(10485760 * 4);
    dbenv->set_lg_max(10485760);
    dbenv->set_lk_max_locks(10000);
    dbenv->set_lk_max_objects(10000);
    dbenv->set_flags(DB_AUTO_COMMIT, 1);
    dbenv->log_set_config(DB_LOG_IN_MEMORY, 1);
    int ret = dbenv->open(nullptr,
                         DB_CREATE |
                             DB_INIT_LOCK |
                             DB_INIT_LOG |
                             DB_INIT_MPOOL |
                             DB_INIT_TXN |
                             DB_THREAD |
                             DB_PRIVATE,
                         S_IRUSR | S_IWUSR);
    if (ret > 0)
        throw std::runtime_error(strprintf("BerkeleyEnvironment::MakeMock: Error %d opening database environment.", ret));

    fDbEnvInit = true;
    fMockDb = true;
}

//
// BerkeleyBatch
//

void BerkeleyEnvironment::Close()
{
    if (!fDbEnvInit)
        return;

    fDbEnvInit = false;

    for (auto& db : mapDb) {
        auto count = mapFileUseCount.find(db.first);
        assert(count == mapFileUseCount.end() || count->second == 0);
        if (db.second) {
            db.second->close(0);
            delete db.second;
            db.second = nullptr;
        }
    }

    int ret = dbenv->close(0);
    if (ret != 0)
        LogPrintf("BerkeleyEnvironment::Close: Error %d closing database environment: %s\n", ret, DbEnv::strerror(ret));
    if (!fMockDb)
        DbEnv((u_int32_t)0).remove(strPath.c_str(), 0);
}

void BerkeleyEnvironment::Reset()
{
    dbenv.reset(new DbEnv(DB_CXX_NO_EXCEPTIONS));
    fDbEnvInit = false;
    fMockDb = false;
}

BerkeleyEnvironment::BerkeleyEnvironment(const fs::path& dir_path) : strPath(dir_path.string())
{
    Reset();
}

BerkeleyEnvironment::~BerkeleyEnvironment()
{
    Close();
}

bool BerkeleyEnvironment::Open(bool retry)
{
    if (fDbEnvInit)
        return true;

    boost::this_thread::interruption_point();

    fs::path pathIn = strPath;
    if (pathIn.empty()) {
        LogPrintf("BerkeleyEnvironment::Open: Error: Environment path is empty!\n");
        return false;
    }
    LogPrintf("BerkeleyEnvironment::Open: Opening database environment at %s\n", pathIn.string());
    TryCreateDirectories(pathIn);
    if (!LockDirectory(pathIn, ".walletlock")) {
        LogPrintf("Cannot obtain a lock on wallet directory %s. Another instance of xpchain may be using it.\n", strPath);
        return false;
    }

    fs::path pathLogDir = pathIn / "database";
    TryCreateDirectories(pathLogDir);
    fs::path pathErrorFile = pathIn / "db.log";
    LogPrintf("BerkeleyEnvironment::Open: LogDir=%s ErrorFile=%s\n", pathLogDir.string(), pathErrorFile.string());

    unsigned int nEnvFlags = 0;
    if (gArgs.GetBoolArg("-privdb", DEFAULT_WALLET_PRIVDB))
        nEnvFlags |= DB_PRIVATE;

    dbenv->set_lg_dir(pathLogDir.string().c_str());
    dbenv->set_cachesize(0, 16 * 1024 * 1024, 1); // Increased from 1 MiB to 16 MiB
    dbenv->set_lg_bsize(1024 * 1024);            // Increased from 64 KiB to 1 MiB
    dbenv->set_lg_max(10 * 1024 * 1024);         // Increased from 1 MiB to 10 MiB
    dbenv->set_lk_max_locks(40000);
    dbenv->set_lk_max_objects(40000);
    dbenv->set_errfile(fsbridge::fopen(pathErrorFile, "a")); /// debug
    dbenv->set_flags(DB_AUTO_COMMIT, 1);
    dbenv->set_flags(DB_TXN_WRITE_NOSYNC, 1);
    dbenv->log_set_config(DB_LOG_AUTO_REMOVE, 1);
    int ret = dbenv->open(strPath.c_str(),
                         DB_CREATE |
                             DB_INIT_LOCK |
                             DB_INIT_LOG |
                             DB_INIT_MPOOL |
                             DB_INIT_TXN |
                             DB_THREAD |
                             DB_RECOVER |
                             nEnvFlags,
                         S_IRUSR | S_IWUSR);
    if (ret != 0) {
        LogPrintf("BerkeleyEnvironment::Open: Error %d opening database environment: %s\n", ret, DbEnv::strerror(ret));
        int ret2 = dbenv->close(0);
        if (ret2 != 0) {
            LogPrintf("BerkeleyEnvironment::Open: Error %d closing failed database environment: %s\n", ret2, DbEnv::strerror(ret2));
        }
        if (retry) {
            // try moving the database env out of the way
            fs::path pathDatabaseBak = pathIn / strprintf("database.%d.bak", GetTime());
            try {
                fs::rename(pathLogDir, pathDatabaseBak);
                LogPrintf("Moved old %s to %s. Retrying.\n", pathLogDir.string(), pathDatabaseBak.string());
            } catch (const fs::filesystem_error&) {
                // failure is ok (well, not really, but it's not worse than what we started with)
            }
            return Open(false);
        }
        return false;
    }

    fDbEnvInit = true;
    return true;
}

BerkeleyEnvironment::VerifyResult BerkeleyEnvironment::Verify(const std::string& strFile, recoverFunc_type recoverFunc, std::string& out_backup_filename)
{
    LOCK(cs_db);
    assert(mapFileUseCount.count(strFile) == 0);

    fs::path pathFile = Directory() / strFile;
    if (!fs::exists(pathFile))
        return VerifyResult::VERIFY_OK;

    Db db(dbenv.get(), 0);
    int result = db.verify(strFile.c_str(), nullptr, nullptr, 0);
    if (result == 0)
        return VerifyResult::VERIFY_OK;

    if (recoverFunc == nullptr)
        return VerifyResult::RECOVER_FAIL;

    // Try to recover:
    bool fCheckFull = recoverFunc(fs::path(strPath) / strFile, out_backup_filename);
    if (fCheckFull)
        return VerifyResult::RECOVER_OK;

    return VerifyResult::RECOVER_FAIL;
}

void BerkeleyDatabase::Open()
{
    if (env) env->Open(true);
}

void BerkeleyDatabase::Close()
{
    if (env) env->Close();
}

void BerkeleyDatabase::Flush()
{
    if (env) env->Flush(true);
}

std::unique_ptr<DatabaseBatch> BerkeleyDatabase::MakeBatch(bool flush_on_close)
{
    return std::make_unique<BerkeleyBatch>(*this, "r+c", flush_on_close);
}

bool BerkeleyBatch::ReadKey(CDataStream&& key, CDataStream& value)
{
    if (!pdb) return false;
    Dbt datKey(key.data(), key.size());
    Dbt datValue;
    datValue.set_flags(DB_DBT_MALLOC);
    int ret = pdb->get(activeTxn, &datKey, &datValue, 0);
    if (ret == 0 && datValue.get_data() != nullptr) {
        value.clear();
        value.write((char*)datValue.get_data(), datValue.get_size());
        memory_cleanse(datValue.get_data(), datValue.get_size());
        free(datValue.get_data());
        return true;
    }
    return false;
}

bool BerkeleyBatch::WriteKey(CDataStream&& key, CDataStream&& value, bool overwrite)
{
    if (!pdb) return true;
    if (fReadOnly) assert(!"Write called on database in read-only mode");
    Dbt datKey(key.data(), key.size());
    Dbt datValue(value.data(), value.size());
    int ret = pdb->put(activeTxn, &datKey, &datValue, (overwrite ? 0 : DB_NOOVERWRITE));
    return (ret == 0);
}

bool BerkeleyBatch::EraseKey(CDataStream&& key)
{
    if (!pdb) return false;
    if (fReadOnly) assert(!"Erase called on database in read-only mode");
    Dbt datKey(key.data(), key.size());
    int ret = pdb->del(activeTxn, &datKey, 0);
    return (ret == 0 || ret == DB_NOTFOUND);
}

bool BerkeleyBatch::HasKey(CDataStream&& key)
{
    if (!pdb) return false;
    Dbt datKey(key.data(), key.size());
    int ret = pdb->exists(activeTxn, &datKey, 0);
    return (ret == 0);
}

std::unique_ptr<DatabaseCursor> BerkeleyBatch::GetCursor()
{
    if (!pdb) return nullptr;
    Dbc* pcursor = nullptr;
    int ret = pdb->cursor(activeTxn, &pcursor, 0);
    if (ret != 0) return nullptr;
    return std::make_unique<BerkeleyCursor>(pcursor);
}

int BerkeleyCursor::Read(CDataStream& ssKey, CDataStream& ssValue)
{
    if (!m_cursor) return -1;
    Dbt datKey;
    Dbt datValue;
    int ret = m_cursor->get(&datKey, &datValue, DB_NEXT);
    if (ret == 0) {
        ssKey.clear();
        ssKey.write((char*)datKey.get_data(), datKey.get_size());
        ssValue.clear();
        ssValue.write((char*)datValue.get_data(), datValue.get_size());
    }
    return ret;
}

bool BerkeleyBatch::Recover(const fs::path& file_path, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& newFilename)
{
    std::string filename;
    BerkeleyEnvironment* env = GetWalletEnv(file_path, filename);

    // Recovery procedure:
    // move wallet file to walletfilename.timestamp.bak
    // Call Salvage with fAggressive=true to
    // get as much data as possible.
    // Rewrite salvaged data to fresh wallet file
    // Set -rescan so any missing transactions will be
    // found.
    int64_t now = GetTime();
    newFilename = strprintf("%s.%d.bak", filename, now);

    int result = env->dbenv->dbrename(nullptr, filename.c_str(), nullptr,
                                       newFilename.c_str(), DB_AUTO_COMMIT);
    if (result == 0)
        LogPrintf("Renamed %s to %s\n", filename, newFilename);
    else
    {
        LogPrintf("Failed to rename %s to %s\n", filename, newFilename);
        return false;
    }

    std::vector<BerkeleyEnvironment::KeyValPair> salvagedData;
    bool fSuccess = env->Salvage(newFilename, true, salvagedData);
    if (salvagedData.empty())
    {
        LogPrintf("Salvage(aggressive) found no records in %s.\n", newFilename);
        return false;
    }
    LogPrintf("Salvage(aggressive) found %u records\n", salvagedData.size());

    std::unique_ptr<Db> pdbCopy = MakeUnique<Db>(env->dbenv.get(), 0);
    int ret = pdbCopy->open(nullptr,               // Txn pointer
                            filename.c_str(),   // Filename
                            "main",             // Logical db name
                            DB_BTREE,           // Database type
                            DB_CREATE,          // Flags
                            0);
    if (ret > 0) {
        LogPrintf("Cannot create database file %s\n", filename);
        pdbCopy->close(0);
        return false;
    }

    DbTxn* ptxn = env->TxnBegin();
    for (BerkeleyEnvironment::KeyValPair& row : salvagedData)
    {
        if (recoverKVcallback)
        {
            CDataStream ssKey(row.first, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(row.second, SER_DISK, CLIENT_VERSION);
            if (!(*recoverKVcallback)(callbackDataIn, ssKey, ssValue))
                continue;
        }
        Dbt datKey(&row.first[0], row.first.size());
        Dbt datValue(&row.second[0], row.second.size());
        int ret2 = pdbCopy->put(ptxn, &datKey, &datValue, DB_NOOVERWRITE);
        if (ret2 > 0)
            fSuccess = false;
    }
    ptxn->commit(0);
    pdbCopy->close(0);

    return fSuccess;
}

bool BerkeleyBatch::VerifyEnvironment(const fs::path& file_path, std::string& errorStr)
{
    std::string walletFile;
    BerkeleyEnvironment* env = GetWalletEnv(file_path, walletFile);
    fs::path walletDir = env->Directory();

    LogPrintf("Using BerkeleyDB version %s\n", DbEnv::version(0, 0, 0));
    LogPrintf("Using wallet %s\n", walletFile);

    // Wallet file must be a plain filename without a directory
    if (fs::path(walletFile).filename().string() != walletFile)
    {
        errorStr = strprintf(_("Wallet %s resides outside wallet directory %s"), walletFile, walletDir.string());
        return false;
    }

    if (!env->Open(true /* retry */)) {
        errorStr = strprintf(_("Error initializing wallet database environment %s!"), walletDir);
        return false;
    }

    return true;
}

bool BerkeleyBatch::VerifyDatabaseFile(const fs::path& file_path, std::string& warningStr, std::string& errorStr, BerkeleyEnvironment::recoverFunc_type recoverFunc)
{
    std::string walletFile;
    BerkeleyEnvironment* env = GetWalletEnv(file_path, walletFile);
    fs::path walletDir = env->Directory();

    if (fs::exists(walletDir / walletFile))
    {
        std::string backup_filename;
        BerkeleyEnvironment::VerifyResult r = env->Verify(walletFile, recoverFunc, backup_filename);
        if (r == BerkeleyEnvironment::VerifyResult::RECOVER_OK)
        {
            warningStr = strprintf(_("Warning: Wallet file corrupt, data salvaged!"
                                     " Original %s saved as %s in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."),
                                   walletFile, backup_filename, walletDir);
        }
        if (r == BerkeleyEnvironment::VerifyResult::RECOVER_FAIL)
        {
            errorStr = strprintf(_("%s corrupt, salvage failed"), walletFile);
            return false;
        }
    }
    // also return true if files does not exists
    return true;
}

/* End of headers, beginning of key/value data */
static const char *HEADER_END = "HEADER=END";
/* End of key/value data */
static const char *DATA_END = "DATA=END";

bool BerkeleyEnvironment::Salvage(const std::string& strFile, bool fAggressive, std::vector<BerkeleyEnvironment::KeyValPair>& vResult)
{
    LOCK(cs_db);
    assert(mapFileUseCount.count(strFile) == 0);

    u_int32_t flags = DB_SALVAGE;
    if (fAggressive)
        flags |= DB_AGGRESSIVE;

    std::stringstream strDump;

    Db db(dbenv.get(), 0);
    int result = db.verify(strFile.c_str(), nullptr, &strDump, flags);
    if (result == DB_VERIFY_BAD) {
        LogPrintf("BerkeleyEnvironment::Salvage: Database salvage found errors, all data may not be recoverable.\n");
        if (!fAggressive) {
            LogPrintf("BerkeleyEnvironment::Salvage: Rerun with aggressive mode to ignore errors and continue.\n");
            return false;
        }
    }
    if (result != 0 && result != DB_VERIFY_BAD) {
        LogPrintf("BerkeleyEnvironment::Salvage: Database salvage failed with result %d.\n", result);
        return false;
    }

    // Format of bdb dump is ascii lines:
    // header lines...
    // HEADER=END
    //  hexadecimal key
    //  hexadecimal value
    //  ... repeated
    // DATA=END

    std::string strLine;
    while (!strDump.eof() && strLine != HEADER_END)
        getline(strDump, strLine); // Skip past header

    std::string keyHex, valueHex;
    while (!strDump.eof() && keyHex != DATA_END) {
        getline(strDump, keyHex);
        if (keyHex != DATA_END) {
            if (strDump.eof())
                break;
            getline(strDump, valueHex);
            if (valueHex == DATA_END) {
                LogPrintf("BerkeleyEnvironment::Salvage: WARNING: Number of keys in data does not match number of values.\n");
                break;
            }
            vResult.push_back(make_pair(ParseHex(keyHex), ParseHex(valueHex)));
        }
    }

    if (keyHex != DATA_END) {
        LogPrintf("BerkeleyEnvironment::Salvage: WARNING: Unexpected end of file while reading salvage output.\n");
        return false;
    }

    return (result == 0);
}


void BerkeleyEnvironment::CheckpointLSN(const std::string& strFile)
{
    dbenv->txn_checkpoint(0, 0, 0);
    if (fMockDb)
        return;
    dbenv->lsn_reset(strFile.c_str(), 0);
}


BerkeleyBatch::BerkeleyBatch(class BerkeleyDatabase& db_in, const char* pszMode, bool fFlushOnCloseIn) : pdb(nullptr), activeTxn(nullptr)
{
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    fFlushOnClose = fFlushOnCloseIn;
    env = db_in.env;
    if (db_in.IsDummy()) {
        return;
    }
    const std::string &strFilename = db_in.strFile;

    bool fCreate = strchr(pszMode, 'c') != nullptr;
    unsigned int nFlags = DB_THREAD;
    if (fCreate)
        nFlags |= DB_CREATE;

    {
        LOCK(cs_db);
        if (!env->Open(false /* retry */))
            throw std::runtime_error("BerkeleyBatch: Failed to open database environment.");

        pdb = env->mapDb[strFilename];
        if (pdb == nullptr) {
            int ret;
            std::unique_ptr<Db> pdb_temp = MakeUnique<Db>(env->dbenv.get(), 0);

            bool fMockDb = env->IsMock();
            if (fMockDb) {
                DbMpoolFile* mpf = pdb_temp->get_mpf();
                ret = mpf->set_flags(DB_MPOOL_NOFILE, 1);
                if (ret != 0) {
                    throw std::runtime_error(strprintf("BerkeleyBatch: Failed to configure for no temp file backing for database %s", strFilename));
                }
            }

            ret = pdb_temp->open(nullptr,                             // Txn pointer
                            fMockDb ? nullptr : strFilename.c_str(),  // Filename
                            fMockDb ? strFilename.c_str() : "main",   // Logical db name
                            DB_BTREE,                                 // Database type
                            nFlags,                                   // Flags
                            0);

            if (ret != 0) {
                throw std::runtime_error(strprintf("BerkeleyBatch: Error %d, can't open database %s", ret, strFilename));
            }

            // Call CheckUniqueFileid on the containing BDB environment to
            // avoid BDB data consistency bugs that happen when different data
            // files in the same environment have the same fileid.
            //
            // Also call CheckUniqueFileid on all the other g_dbenvs to prevent
            // xpchain from opening the same data file through another
            // environment when the file is referenced through equivalent but
            // not obviously identical symlinked or hard linked or bind mounted
            // paths. In the future a more relaxed check for equal inode and
            // device ids could be done instead, which would allow opening
            // different backup copies of a wallet at the same time. Maybe even
            // more ideally, an exclusive lock for accessing the database could
            // be implemented, so no equality checks are needed at all. (Newer
            // versions of BDB have an set_lk_exclusive method for this
            // purpose, but the older version we use does not.)
            for (auto& env : g_dbenvs) {
                CheckUniqueFileid(env.second, strFilename, *pdb_temp);
            }

            pdb = pdb_temp.release();
            env->mapDb[strFilename] = pdb;

            if (fCreate) {
                CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                ssKey << std::string("version");
                if (!HasKey(std::move(ssKey))) {
                    CDataStream ssVersion(SER_DISK, CLIENT_VERSION);
                    ssVersion << CLIENT_VERSION;
                    CDataStream ssKey2(SER_DISK, CLIENT_VERSION);
                    ssKey2 << std::string("version");
                    fReadOnly = false;
                    WriteKey(std::move(ssKey2), std::move(ssVersion));
                    fReadOnly = true;
                }
            }
        }
        ++env->mapFileUseCount[strFilename];
        strFile = strFilename;
    }
}

void BerkeleyBatch::Flush()
{
    if (activeTxn)
        return;

    // Flush database activity from memory pool to disk log
    unsigned int nMinutes = 0;
    if (fReadOnly)
        nMinutes = 1;

    env->dbenv->txn_checkpoint(nMinutes ? gArgs.GetArg("-dblogsize", DEFAULT_WALLET_DBLOGSIZE) * 1024 : 0, nMinutes, 0);
}

void BerkeleyDatabase::IncrementUpdateCounter()
{
    ++nUpdateCounter;
}

bool BerkeleyDatabase::PeriodicFlush()
{
    return BerkeleyBatch::PeriodicFlush(*this);
}

void BerkeleyBatch::Close()
{
    if (!pdb)
        return;
    if (activeTxn)
        activeTxn->abort();
    activeTxn = nullptr;
    pdb = nullptr;

    if (fFlushOnClose)
        Flush();

    {
        LOCK(cs_db);
        --env->mapFileUseCount[strFile];
    }
}

void BerkeleyEnvironment::CloseDb(const std::string& strFile)
{
    {
        LOCK(cs_db);
        if (mapDb[strFile] != nullptr) {
            // Close the database handle
            Db* pdb = mapDb[strFile];
            pdb->close(0);
            delete pdb;
            mapDb[strFile] = nullptr;
        }
    }
}

bool BerkeleyBatch::Rewrite(BerkeleyDatabase& database, const char* pszSkip)
{
    if (database.IsDummy()) {
        return true;
    }
    BerkeleyEnvironment *env = database.env;
    const std::string& strFile = database.strFile;
    while (true) {
        {
            LOCK(cs_db);
            if (!env->mapFileUseCount.count(strFile) || env->mapFileUseCount[strFile] == 0) {
                // Flush log data to the dat file
                env->CloseDb(strFile);
                env->CheckpointLSN(strFile);
                env->mapFileUseCount.erase(strFile);

                bool fSuccess = true;
                LogPrintf("BerkeleyBatch::Rewrite: Rewriting %s...\n", strFile);
                std::string strFileRes = strFile + ".rewrite";
                { // surround usage of db with extra {}
                    BerkeleyBatch db(database, "r");
                    std::unique_ptr<Db> pdbCopy = MakeUnique<Db>(env->dbenv.get(), 0);

                    int ret = pdbCopy->open(nullptr,               // Txn pointer
                                            strFileRes.c_str(), // Filename
                                            "main",             // Logical db name
                                            DB_BTREE,           // Database type
                                            DB_CREATE,          // Flags
                                            0);
                    if (ret > 0) {
                        LogPrintf("BerkeleyBatch::Rewrite: Can't create database file %s\n", strFileRes);
                        fSuccess = false;
                    }

                    std::unique_ptr<DatabaseCursor> pcursor = db.GetCursor();
                    if (pcursor)
                        while (fSuccess) {
                            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                            int ret1 = pcursor->Read(ssKey, ssValue);
                            if (ret1 == -1) {
                                break;
                            } else if (ret1 != 0) {
                                // ret1 is 0 for success in DB_NEXT, DB_NOTFOUND is special but here let's assume -1 is end or error
                                // Actually BerkeleyDB Dbc::get returns DB_NOTFOUND when done.
                                // My Read() implementation returns ret from m_cursor->get.
                                if (ret1 == DB_NOTFOUND) break;
                                fSuccess = false;
                                break;
                            }
                            if (pszSkip &&
                                strncmp(ssKey.data(), pszSkip, std::min(ssKey.size(), strlen(pszSkip))) == 0)
                                continue;
                            if (strncmp(ssKey.data(), "\x07version", 8) == 0) {
                                // Update version:
                                ssValue.clear();
                                ssValue << CLIENT_VERSION;
                            }
                            Dbt datKey(ssKey.data(), ssKey.size());
                            Dbt datValue(ssValue.data(), ssValue.size());
                            int ret2 = pdbCopy->put(nullptr, &datKey, &datValue, DB_NOOVERWRITE);
                            if (ret2 > 0)
                                fSuccess = false;
                        }
                    pcursor.reset();
                    if (fSuccess) {
                        db.Close();
                        env->CloseDb(strFile);
                        if (pdbCopy->close(0))
                            fSuccess = false;
                    } else {
                        pdbCopy->close(0);
                    }
                }
                if (fSuccess) {
                    Db dbA(env->dbenv.get(), 0);
                    if (dbA.remove(strFile.c_str(), nullptr, 0))
                        fSuccess = false;
                    Db dbB(env->dbenv.get(), 0);
                    if (dbB.rename(strFileRes.c_str(), nullptr, strFile.c_str(), 0))
                        fSuccess = false;
                }
                if (!fSuccess)
                    LogPrintf("BerkeleyBatch::Rewrite: Failed to rewrite database file %s\n", strFileRes);
                return fSuccess;
            }
        }
        MilliSleep(100);
    }
}


void BerkeleyEnvironment::Flush(bool fShutdown)
{
    int64_t nStart = GetTimeMillis();
    // Flush log data to the actual data file on all files that are not in use
    LogPrint(BCLog::DB, "BerkeleyEnvironment::Flush: Flush(%s)%s\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " database not started");
    if (!fDbEnvInit)
        return;
    {
        LOCK(cs_db);
        std::map<std::string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end()) {
            std::string strFile = (*mi).first;
            int nRefCount = (*mi).second;
            LogPrint(BCLog::DB, "BerkeleyEnvironment::Flush: Flushing %s (refcount = %d)...\n", strFile, nRefCount);
            if (nRefCount == 0) {
                // Move log data to the dat file
                CloseDb(strFile);
                LogPrint(BCLog::DB, "BerkeleyEnvironment::Flush: %s checkpoint\n", strFile);
                dbenv->txn_checkpoint(0, 0, 0);
                LogPrint(BCLog::DB, "BerkeleyEnvironment::Flush: %s detach\n", strFile);
                if (!fMockDb)
                    dbenv->lsn_reset(strFile.c_str(), 0);
                LogPrint(BCLog::DB, "BerkeleyEnvironment::Flush: %s closed\n", strFile);
                mapFileUseCount.erase(mi++);
            } else
                mi++;
        }
        LogPrint(BCLog::DB, "BerkeleyEnvironment::Flush: Flush(%s)%s took %15dms\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " database not started", GetTimeMillis() - nStart);
        if (fShutdown) {
            char** listp;
            if (mapFileUseCount.empty()) {
                dbenv->log_archive(&listp, DB_ARCH_REMOVE);
                Close();
                if (!fMockDb) {
                    fs::remove_all(fs::path(strPath) / "database");
                }
                g_dbenvs.erase(strPath);
            }
        }
    }
}

bool BerkeleyBatch::PeriodicFlush(class BerkeleyDatabase& db_in)
{
    if (db_in.IsDummy()) {
        return true;
    }
    bool ret = false;
    BerkeleyEnvironment *env = db_in.env;
    const std::string& strFile = db_in.strFile;
    TRY_LOCK(cs_db, lockDb);
    if (lockDb)
    {
        // Don't do this if any databases are in use
        int nRefCount = 0;
        std::map<std::string, int>::iterator mit = env->mapFileUseCount.begin();
        while (mit != env->mapFileUseCount.end())
        {
            nRefCount += (*mit).second;
            mit++;
        }

        if (nRefCount == 0)
        {
            boost::this_thread::interruption_point();
            std::map<std::string, int>::iterator mi = env->mapFileUseCount.find(strFile);
            if (mi != env->mapFileUseCount.end())
            {
                LogPrint(BCLog::DB, "Flushing %s\n", strFile);
                int64_t nStart = GetTimeMillis();

                // Flush wallet file so it's self contained
                env->CloseDb(strFile);
                env->CheckpointLSN(strFile);

                env->mapFileUseCount.erase(mi++);
                LogPrint(BCLog::DB, "Flushed %s %dms\n", strFile, GetTimeMillis() - nStart);
                ret = true;
            }
        }
    }

    return ret;
}

bool BerkeleyDatabase::Rewrite(const char* pszSkip)
{
    return BerkeleyBatch::Rewrite(*this, pszSkip);
}

bool BerkeleyDatabase::Backup(const std::string& strDest) const
{
    if (IsDummy()) {
        return false;
    }
    while (true)
    {
        {
            LOCK(cs_db);
            if (!env->mapFileUseCount.count(strFile) || env->mapFileUseCount[strFile] == 0)
            {
                // Flush log data to the dat file
                env->CloseDb(strFile);
                env->CheckpointLSN(strFile);
                env->mapFileUseCount.erase(strFile);

                // Copy wallet file
                fs::path pathSrc = env->Directory() / strFile;
                fs::path pathDest(strDest);
                if (fs::is_directory(pathDest))
                    pathDest /= strFile;

                try {
                    if (fs::equivalent(pathSrc, pathDest)) {
                        LogPrintf("cannot backup to wallet source file %s\n", pathDest.string());
                        return false;
                    }

                    if (fs::exists(pathDest)) {
                        fs::remove(pathDest);
                    }
                    fs::copy_file(pathSrc, pathDest);
                    LogPrintf("copied %s to %s\n", strFile, pathDest.string());
                    return true;
                } catch (const fs::filesystem_error& e) {
                    LogPrintf("error copying %s to %s - %s\n", strFile, pathDest.string(), e.what());
                    return false;
                }
            }
        }
        MilliSleep(100);
    }
    return false;
}

#ifdef USE_SQLITE
#include <wallet/sqlite.h>
#endif

bool IsBerkeleyBDBFile(const fs::path& path)
{
    if (!fs::is_regular_file(path)) return false;
    FILE* f = fsbridge::fopen(path, "rb");
    if (!f) return false;
    
    unsigned char magic[16];
    size_t read_bytes = fread(magic, 1, 16, f);
    fclose(f);
    
    if (read_bytes < 16) return false;
    
    uint32_t bdb_magic = ((uint32_t)magic[12] << 24) | ((uint32_t)magic[13] << 16) | ((uint32_t)magic[14] << 8) | magic[15];
    uint32_t bdb_magic_le = ((uint32_t)magic[15] << 24) | ((uint32_t)magic[14] << 16) | ((uint32_t)magic[13] << 8) | magic[12];
    
    // Btree magic: 0x00061561, Hash magic: 0x00053162
    return (bdb_magic == 0x00061561 || bdb_magic_le == 0x00061561 ||
            bdb_magic == 0x00053162 || bdb_magic_le == 0x00053162);
}

std::unique_ptr<WalletDatabase> CreateWalletDatabase(const fs::path& path, uint64_t wallet_creation_flags)
{
    bool is_sqlite = false;
    bool is_bdb = false;
    if (fs::is_regular_file(path)) {
        FILE* f = fsbridge::fopen(path, "rb");
        if (f) {
            char magic[16];
            if (fread(magic, 1, 16, f) == 16) {
                if (memcmp(magic, "SQLite format 3\000", 16) == 0) {
                    is_sqlite = true;
                }
            }
            fclose(f);
        }
        
        // SQLite가 아닌 경우에만 Berkeley DB 여부를 추가 판별
        if (!is_sqlite) {
            is_bdb = IsBerkeleyBDBFile(path);
        }
    }

    // WALLET_FLAG_DESCRIPTORS implies SQLite. 
    // New wallets with this flag or existing SQLite files (including encrypted ones) will use SQLiteDatabase.
    // 단, 명시적으로 Berkeley DB 파일로 감지된 경우에는 BDB 환경으로 처리합니다.
    if (!is_bdb && (is_sqlite || (wallet_creation_flags & WALLET_FLAG_DESCRIPTORS) || 
        path.extension() == ".sqlite" || path.filename() == "wallet.dat")) {
        // 만약 매직바이트는 없지만 확장자가 sqlite이거나, 
        // 이미 sqlite로 알려진 파일이라면 암호화된 상태일 가능성이 큽니다.
#ifdef USE_SQLITE
        return std::make_unique<SQLiteDatabase>(path.string());
#else
        LogPrintf("CreateWalletDatabase: SQLite support not enabled but requested!\n");
        throw std::runtime_error("SQLite support not enabled");
#endif
    }
    return std::make_unique<BerkeleyDatabase>(path);
}

bool BerkeleyDatabase::CopyRecordsTo(WalletDatabase& dest, std::string& error)
{
#ifndef USE_SQLITE
    error = "SQLite support is not enabled";
    return false;
#else
    if (dest.Format() != "sqlite") {
        error = "Destination database must be SQLite format";
        return false;
    }
    if (IsDummy() || !env) {
        error = "Source database is not open";
        return false;
    }

    dest.Open();
    auto dest_batch = dest.MakeBatch(true);
    if (!dest_batch) {
        error = "Failed to open destination database batch";
        return false;
    }
    if (!dest_batch->TxnBegin()) {
        error = "Failed to begin destination transaction";
        return false;
    }

    BerkeleyBatch src_batch(*this, "r", false);
    std::unique_ptr<DatabaseCursor> pcursor = src_batch.GetCursor();
    if (!pcursor) {
        error = "Failed to create source database cursor";
        dest_batch->TxnAbort();
        return false;
    }

    int count = 0;
    while (true) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = pcursor->Read(ssKey, ssValue);
        if (ret == DB_NOTFOUND) {
            break;
        }
        if (ret != 0) {
            error = "Error reading source wallet database";
            dest_batch->TxnAbort();
            return false;
        }
        if (!dest_batch->WriteKey(std::move(ssKey), std::move(ssValue), false)) {
            error = "Error writing destination wallet database";
            dest_batch->TxnAbort();
            return false;
        }
        ++count;
    }

    if (!dest_batch->TxnCommit()) {
        error = "Failed to commit destination transaction";
        return false;
    }

    dest.Flush();
    LogPrintf("BerkeleyDatabase::CopyRecordsTo: copied %d records to %s\n", count, dest.Filename());
    return count > 0;
#endif
}

bool CopyWalletDatabase(WalletDatabase& src, WalletDatabase& dest, std::string& error)
{
#ifndef USE_SQLITE
    error = "SQLite support is not enabled";
    return false;
#else
    if (src.Format() == "berkeley") {
        BerkeleyDatabase& bdb = static_cast<BerkeleyDatabase&>(src);
        return bdb.CopyRecordsTo(dest, error);
    }

    if (dest.Format() != "sqlite") {
        error = "Destination database must be SQLite format";
        return false;
    }

    src.Open();
    dest.Open();

    auto src_batch = src.MakeBatch(false);
    auto dest_batch = dest.MakeBatch(true);
    if (!src_batch || !dest_batch) {
        error = "Failed to open database batch";
        return false;
    }

    if (!dest_batch->TxnBegin()) {
        error = "Failed to begin destination transaction";
        return false;
    }

    std::unique_ptr<DatabaseCursor> pcursor = src_batch->GetCursor();
    if (!pcursor) {
        error = "Failed to create source database cursor";
        dest_batch->TxnAbort();
        return false;
    }

    int count = 0;
    while (true) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = pcursor->Read(ssKey, ssValue);
        if (ret == DB_NOTFOUND) {
            break;
        }
        if (ret != 0) {
            error = "Error reading source wallet database";
            dest_batch->TxnAbort();
            return false;
        }
        if (!dest_batch->WriteKey(std::move(ssKey), std::move(ssValue), false)) {
            error = "Error writing destination wallet database";
            dest_batch->TxnAbort();
            return false;
        }
        ++count;
    }

    if (!dest_batch->TxnCommit()) {
        error = "Failed to commit destination transaction";
        return false;
    }

    dest.Flush();
    LogPrintf("CopyWalletDatabase: copied %d records from %s (%s) to %s (%s)\n",
        count, src.Filename(), src.Format(), dest.Filename(), dest.Format());
    return count > 0;
#endif
}

bool CopyWalletDatabaseFile(const fs::path& src_path, const fs::path& dest_path, std::string& error)
{
#ifndef USE_SQLITE
    error = "SQLite support is not enabled";
    return false;
#else
    if (!fs::exists(src_path)) {
        error = "Source wallet file not found";
        return false;
    }
    if (IsSQLiteFile(src_path)) {
        error = "Source wallet is already SQLite format";
        return false;
    }
    if (!IsBerkeleyBDBFile(src_path)) {
        error = "Source is not a Berkeley DB wallet file";
        return false;
    }
    if (fs::exists(dest_path)) {
        error = "Destination wallet file already exists";
        return false;
    }
    if (dest_path.extension() != ".sqlite") {
        error = "Destination path must use the .sqlite extension";
        return false;
    }

    std::unique_ptr<WalletDatabase> src_db = CreateWalletDatabase(src_path, 0);
    std::unique_ptr<WalletDatabase> dest_db = CreateWalletDatabase(dest_path, 0);
    return CopyWalletDatabase(*src_db, *dest_db, error);
#endif
}
