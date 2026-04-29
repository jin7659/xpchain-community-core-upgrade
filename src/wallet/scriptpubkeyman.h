// Copyright (c) 2019-2020 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// XPChain adaptation of XPChain Core's ScriptPubKeyMan framework.
// Adapted by Antigravity to be compatible with XPChain's keystore hierarchy
// (CBasicKeyStore / CCryptoKeyStore) and script type system (txnouttype).

#ifndef XPCHAIN_WALLET_SCRIPTPUBKEYMAN_H
#define XPCHAIN_WALLET_SCRIPTPUBKEYMAN_H

#include <coins.h>
#include <keystore.h>
#include <outputtype.h>
#include <script/descriptor.h>
#include <script/ismine.h>
#include <script/sign.h>
#include <script/standard.h>
#include <util/error.h>
#include <wallet/crypter.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

#include <boost/signals2/signal.hpp>
#include <memory>
#include <unordered_map>

// ─── Forward declarations ────────────────────────────────────────────────────
// Note: XPChain uses std::string for error messages instead of bilingual_str
enum class OutputType;

// ─── Enum types needed by ScriptPubKeyMan ───────────────────────────────────

/** Result of a signing operation */
enum class SigningResult {
    OK,                        //!< Signing succeeded
    PRIVATE_KEY_NOT_AVAILABLE, //!< Private key for address is not available
    SIGNING_FAILED,            //!< Signing failed for another reason
};

/** Transaction-level error codes */
enum class TransactionError {
    OK,               //!< No error
    MISSING_INPUTS,   //!< Inputs not available in wallet
    SIGHASH_MISMATCH, //!< Sighash type mismatch
    INVALID_PSBT,     //!< PSBT is invalid
};

// ─── WalletStorage interface ─────────────────────────────────────────────────

/**
 * WalletStorage gives ScriptPubKeyMan access to wallet-level state without
 * introducing a circular dependency on CWallet.
 */
class WalletStorage
{
public:
    virtual ~WalletStorage() = default;
    virtual const std::string GetDisplayName() const = 0;
    virtual WalletDatabase& GetDatabase() const = 0;
    virtual bool IsWalletFlagSet(uint64_t flag) const = 0;
    virtual void UnsetBlankWalletFlag(WalletBatch&) = 0;
    virtual bool CanSupportFeature(int feature_version) const = 0;
    virtual void SetMinVersion(int version, WalletBatch* = nullptr) = 0;
    virtual const CKeyingMaterial& GetEncryptionKey() const = 0;
    virtual bool HasEncryptionKeys() const = 0;
    virtual bool IsLocked() const = 0;
};

// ─── Constants ───────────────────────────────────────────────────────────────
// Note: DEFAULT_KEYPOOL_SIZE is defined in wallet/wallet.h as 1000

// Wallet flags are now defined in wallet/walletutil.h

/** Hash struct for CKeyID in unordered containers */
struct KeyIDHasher {
    size_t operator()(const CKeyID& id) const {
        return std::hash<std::string>()(std::string(id.begin(), id.end()));
    }
};

// IsFeatureSupported is defined in wallet/walletutil.h

// ─── Helper functions ─────────────────────────────────────────────────────────

std::vector<CKeyID> GetAffectedKeys(const CScript& spk, const SigningProvider& provider);

// ─── ScriptPubKeyMan base class ───────────────────────────────────────────────

/**

 * Abstract base for all ScriptPubKeyMan implementations.
 * Encapsulates script/key management for a CWallet.
 */
class ScriptPubKeyMan
{
protected:
    WalletStorage& m_storage;

public:
    ScriptPubKeyMan(WalletStorage& storage) : m_storage(storage) {}
    virtual ~ScriptPubKeyMan() = default;

    virtual bool GetNewDestination(const OutputType type, CTxDestination& dest, std::string& error) { return false; }
    virtual isminetype IsMine(const CScript& script) const { return ISMINE_NO; }

    virtual bool CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys = false) { return false; }
    virtual bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) { return false; }

    virtual bool GetReservedDestination(const OutputType type, bool internal, CTxDestination& address, int64_t& index, CKeyPool& keypool) { return false; }
    virtual void KeepDestination(int64_t index, const OutputType& type) {}
    virtual void ReturnDestination(int64_t index, bool internal, const CTxDestination& addr) {}

    virtual bool TopUp(unsigned int size = 0) { return false; }

    virtual void MarkUnusedAddresses(const CScript& script) {}

    virtual bool SetupGeneration(bool force = false) { return false; }

    virtual bool IsHDEnabled() const { return false; }
    virtual bool CanGetAddresses(bool internal = false) const { return false; }

    virtual bool Upgrade(int prev_version, int new_version, std::string& error) { return false; }

    virtual bool HavePrivateKeys() const { return false; }

    virtual void RewriteDB() {}

    virtual int64_t GetOldestKeyPoolTime() const { return GetTime(); }
    virtual size_t KeypoolCountExternalKeys() const { return 0; }
    virtual unsigned int GetKeyPoolSize() const { return 0; }
    virtual int64_t GetTimeFirstKey() const { return 0; }

    virtual std::unique_ptr<CKeyMetadata> GetMetadata(const CTxDestination& dest) const { return nullptr; }
    virtual std::unique_ptr<SigningProvider> GetSolvingProvider(const CScript& script) const { return nullptr; }
    virtual bool CanProvide(const CScript& script, SignatureData& sigdata) { return false; }

    /** Creates new signatures for a transaction. Returns true if all inputs were signed. */
    virtual bool SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins,
                                  int sighash, std::map<int, std::string>& input_errors) const { return false; }

    /** Sign a message with the key behind the given CKeyID */
    virtual SigningResult SignMessage(const std::string& message, const CKeyID& keyid, std::string& str_sig) const {
        return SigningResult::SIGNING_FAILED;
    }

    virtual uint256 GetID() const { return uint256(); }
    virtual void SetInternal(bool internal) {}

    /** Prepends the wallet name in logging output */
    template<typename... Params>
    void WalletLogPrintf(std::string fmt, Params... parameters) const {
        LogPrintf(("%" + std::string("s") + " " + fmt).c_str(), m_storage.GetDisplayName(), parameters...);
    }

    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;
    boost::signals2::signal<void ()> NotifyCanGetAddressesChanged;
};

// ─── Signing-provider wrapper ─────────────────────────────────────────────────

/** A minimal SigningProvider view of a LegacyScriptPubKeyMan — no private keys exposed */
class LegacySigningProvider;  // forward

// ─── LegacyScriptPubKeyMan ────────────────────────────────────────────────────

/**
 * LegacyScriptPubKeyMan manages keys and scripts for legacy HD wallets backed by
 * BerkeleyDB or SQLite. Inherits from CBasicKeyStore (acting as FillableSigningProvider)
 * and CCryptoKeyStore for encrypted key support.
 */
class LegacyScriptPubKeyMan : public ScriptPubKeyMan, public CCryptoKeyStore
{
private:
    bool fDecryptionThoroughlyChecked = true;

    WalletBatch* encrypted_batch GUARDED_BY(cs_KeyStore) = nullptr;

    using CryptedKeyMap = std::map<CKeyID, std::pair<CPubKey, std::vector<unsigned char>>>;

    int64_t nTimeFirstKey GUARDED_BY(cs_KeyStore) = 0;

    bool AddKeyPubKeyInner(const CKey& key, const CPubKey& pubkey);
    bool AddCryptedKeyInner(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);

    bool AddWatchOnly(const CScript& dest) override EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    bool AddWatchOnlyWithDB(WalletBatch& batch, const CScript& dest) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    bool AddWatchOnlyInMem(const CScript& dest);
    bool AddWatchOnlyWithDB(WalletBatch& batch, const CScript& dest, int64_t create_time) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

    bool AddKeyPubKeyWithDB(WalletBatch& batch, const CKey& key, const CPubKey& pubkey) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    void AddKeypoolPubkeyWithDB(const CPubKey& pubkey, const bool internal, WalletBatch& batch);
    bool AddCScriptWithDB(WalletBatch& batch, const CScript& script);
    bool AddKeyOriginWithDB(WalletBatch& batch, const CPubKey& pubkey, const KeyOriginInfo& info);

    CHDChain m_hd_chain;
    std::unordered_map<CKeyID, CHDChain, KeyIDHasher> m_inactive_hd_chains;

    void DeriveNewChildKey(WalletBatch& batch, CKeyMetadata& metadata, CKey& secret,
                           CHDChain& hd_chain, bool internal = false) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

    std::set<int64_t> setInternalKeyPool GUARDED_BY(cs_KeyStore);
    std::set<int64_t> setExternalKeyPool GUARDED_BY(cs_KeyStore);
    std::set<int64_t> set_pre_split_keypool GUARDED_BY(cs_KeyStore);
    int64_t m_max_keypool_index GUARDED_BY(cs_KeyStore) = 0;
    std::map<CKeyID, int64_t> m_pool_key_to_index;
    std::map<int64_t, CKeyID> m_index_to_reserved_key;

    bool GetKeyFromPool(CPubKey& key, const OutputType type, bool internal = false);
    bool ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fRequestedInternal);
    bool TopUpInactiveHDChain(const CKeyID seed_id, int64_t index, bool internal);

public:
    using ScriptPubKeyMan::ScriptPubKeyMan;

    // ── ScriptPubKeyMan overrides ──
    bool GetNewDestination(const OutputType type, CTxDestination& dest, std::string& error) override;
    isminetype IsMine(const CScript& script) const override;

    bool CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys = false) override;
    bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) override;

    bool GetReservedDestination(const OutputType type, bool internal, CTxDestination& address,
                                 int64_t& index, CKeyPool& keypool) override;
    void KeepDestination(int64_t index, const OutputType& type) override;
    void ReturnDestination(int64_t index, bool internal, const CTxDestination&) override;

    bool TopUp(unsigned int size = 0) override;
    void MarkUnusedAddresses(const CScript& script) override;
    void UpgradeKeyMetadata();

    bool IsHDEnabled() const override;
    bool SetupGeneration(bool force = false) override;
    bool Upgrade(int prev_version, int new_version, std::string& error) override;
    bool HavePrivateKeys() const override;
    void RewriteDB() override;

    int64_t GetOldestKeyPoolTime() const override;
    size_t KeypoolCountExternalKeys() const override;
    unsigned int GetKeyPoolSize() const override;
    int64_t GetTimeFirstKey() const override;

    std::unique_ptr<CKeyMetadata> GetMetadata(const CTxDestination& dest) const override;
    bool CanGetAddresses(bool internal = false) const override;

    std::unique_ptr<SigningProvider> GetSolvingProvider(const CScript& script) const override;
    bool CanProvide(const CScript& script, SignatureData& sigdata) override;

    bool SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins,
                          int sighash, std::map<int, std::string>& input_errors) const override;
    SigningResult SignMessage(const std::string& message, const CKeyID& keyid, std::string& str_sig) const override;

    uint256 GetID() const override;
    void SetInternal(bool internal) override;

    // ── Key/script metadata maps ──
    std::map<CKeyID, CKeyMetadata>   mapKeyMetadata    GUARDED_BY(cs_KeyStore);
    std::map<CScriptID, CKeyMetadata> m_script_metadata GUARDED_BY(cs_KeyStore);

    // ── Key management ──
    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey) override;
    bool LoadKey(const CKey& key, const CPubKey& pubkey);
    bool AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret) override;
    bool LoadCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, bool checksum_valid);
    void UpdateTimeFirstKey(int64_t nCreateTime) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    bool LoadCScript(const CScript& redeemScript);
    void LoadKeyMetadata(const CKeyID& keyID, const CKeyMetadata& metadata);
    void LoadScriptMetadata(const CScriptID& script_id, const CKeyMetadata& metadata);

    CPubKey GenerateNewKey(WalletBatch& batch, CHDChain& hd_chain, bool internal = false) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

    void AddHDChain(const CHDChain& chain);
    void LoadHDChain(const CHDChain& chain);
    const CHDChain& GetHDChain() const { return m_hd_chain; }
    void AddInactiveHDChain(const CHDChain& chain);

    bool LoadWatchOnly(const CScript& dest);
    bool HaveWatchOnly(const CScript& dest) const override;
    bool HaveWatchOnly() const override;
    bool RemoveWatchOnly(const CScript& dest) override;
    bool AddWatchOnly(const CScript& dest, int64_t nCreateTime) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

    bool GetWatchPubKey(const CKeyID& address, CPubKey& pubkey_out) const;

    // ── SigningProvider overrides ──
    bool HaveKey(const CKeyID& address) const override;
    bool GetKey(const CKeyID& address, CKey& keyOut) const override;
    bool GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const override;
    bool AddCScript(const CScript& redeemScript) override;
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override;

    void LoadKeyPool(int64_t nIndex, const CKeyPool& keypool);
    bool NewKeyPool();
    void MarkPreSplitKeys() EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

    bool ImportScripts(const std::set<CScript> scripts, int64_t timestamp) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    bool ImportPrivKeys(const std::map<CKeyID, CKey>& privkey_map, const int64_t timestamp) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    bool ImportPubKeys(const std::vector<CKeyID>& ordered_pubkeys,
                        const std::map<CKeyID, CPubKey>& pubkey_map,
                        const std::map<CKeyID, std::pair<CPubKey, KeyOriginInfo>>& key_origins,
                        const bool add_keypool, const bool internal, const int64_t timestamp) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    bool ImportScriptPubKeys(const std::set<CScript>& script_pub_keys,
                              const bool have_solving_data, const int64_t timestamp) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

    bool CanGenerateKeys() const;
    CPubKey GenerateNewSeed();
    CPubKey DeriveNewSeed(const CKey& key);
    void SetHDSeed(const CPubKey& key);

    void LearnRelatedScripts(const CPubKey& key, OutputType);
    void LearnAllRelatedScripts(const CPubKey& key);

    void MarkReserveKeysAsUsed(int64_t keypool_id) EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);
    const std::map<CKeyID, int64_t>& GetAllReserveKeys() const { return m_pool_key_to_index; }

    std::set<CKeyID> GetKeys() const override;
};

// ─── LegacySigningProvider ────────────────────────────────────────────────────

/** Wraps LegacyScriptPubKeyMan to expose only public key information (no private keys). */
class LegacySigningProvider : public SigningProvider
{
private:
    const LegacyScriptPubKeyMan& m_spk_man;
public:
    LegacySigningProvider(const LegacyScriptPubKeyMan& spk_man) : m_spk_man(spk_man) {}

    bool GetCScript(const CScriptID& scriptid, CScript& script) const override {
        return m_spk_man.GetCScript(scriptid, script);
    }
    bool HaveCScript(const CScriptID& scriptid) const override {
        return m_spk_man.HaveCScript(scriptid);
    }
    bool GetPubKey(const CKeyID& address, CPubKey& pubkey) const override {
        return m_spk_man.GetPubKey(address, pubkey);
    }
    bool GetKey(const CKeyID& address, CKey& key) const override {
        return false; // No private keys for this provider
    }
    bool HaveKey(const CKeyID& address) const override {
        return false;
    }
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override {
        return m_spk_man.GetKeyOrigin(keyid, info);
    }
};

/**
 * DescriptorScriptPubKeyMan manages scripts/keys via descriptors (BIP380+).
 * This is the modern standard for wallets in XPChain Core v27.0+.
 */
class DescriptorScriptPubKeyMan : public ScriptPubKeyMan, public SigningProvider
{
private:
    using DescriptorCache = std::map<int, CScript>;
    
    struct DescriptorInfo {
        std::unique_ptr<Descriptor> descriptor;
        DescriptorCache cache;
        bool internal;
        int64_t next_index = 0;
        int64_t range_start = 0;
        int64_t range_end = 0;
    };

    std::map<uint256, DescriptorInfo> m_descriptors;
    mutable CCriticalSection cs_desc_man;

    FlatSigningProvider m_storage_provider;

public:
    DescriptorScriptPubKeyMan(WalletStorage& storage) : ScriptPubKeyMan(storage) {}

    // ── ScriptPubKeyMan overrides ──
    isminetype IsMine(const CScript& script) const override;
    bool CheckDecryptionKey(const CKeyingMaterial& master_key, bool accept_no_keys = false) override { return true; }
    
    bool GetNewDestination(const OutputType type, CTxDestination& dest, std::string& error) override;
    bool SetupGeneration(bool force = false) override;
    
    bool SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins,
                                  int sighash, std::map<int, std::string>& input_errors) const override;
    SigningResult SignMessage(const std::string& message, const CKeyID& keyid, std::string& str_sig) const override;

    uint256 GetID() const override;
    void SetInternal(bool internal) override;

    // ── SigningProvider overrides ──
    bool GetCScript(const CScriptID& scriptid, CScript& script) const override { return m_storage_provider.GetCScript(scriptid, script); }
    bool HaveCScript(const CScriptID& scriptid) const override { return m_storage_provider.HaveCScript(scriptid); }
    bool GetPubKey(const CKeyID& address, CPubKey& pubkey) const override { return m_storage_provider.GetPubKey(address, pubkey); }
    bool GetKey(const CKeyID& address, CKey& key) const override { return m_storage_provider.GetKey(address, key); }
    bool HaveKey(const CKeyID& address) const override { return m_storage_provider.HaveKey(address); }
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override { return m_storage_provider.GetKeyOrigin(keyid, info); }

    // ── Descriptor management ──
    bool AddDescriptor(std::unique_ptr<Descriptor> desc, bool internal);
    const DescriptorCache& GetDescriptorCache(const uint256& id) const;
};

#endif // XPCHAIN_WALLET_SCRIPTPUBKEYMAN_H
