// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETUTIL_H
#define BITCOIN_WALLET_WALLETUTIL_H

#include <chainparamsbase.h>
#include <pubkey.h>
#include <serialize.h>
#include <util.h>

//! Get the path of the wallet directory.
fs::path GetWalletDir();

// ─── Wallet version features ─────────────────────────────────────────────────

//! Default keypool size
static const unsigned int DEFAULT_KEYPOOL_SIZE = 1000;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500,           // earliest version new wallets support
    FEATURE_WALLETCRYPT = 40000,    // wallet encryption
    FEATURE_COMPRPUBKEY = 60000,    // compressed public keys
    FEATURE_HD = 130000,            // HD Wallet (BIP32)
    FEATURE_HD_SPLIT = 139900,      // HD chain split (m/0'/1'/k for change)
    FEATURE_NO_DEFAULT_KEY = 159900,// wallet without a default key
    FEATURE_PRE_SPLIT_KEYPOOL = 169900, // pre-split keypool tracking
    FEATURE_LATEST = FEATURE_PRE_SPLIT_KEYPOOL
};

/** Helper: returns true if a wallet feature version is supported */
static inline bool IsFeatureSupported(int wallet_version, int feature_version)
{
    return wallet_version >= feature_version;
}

// ─── Wallet flags ─────────────────────────────────────────────────────────────

enum WalletFlags : uint64_t {
    // wallet flags in the lower section <= (1 << 31) will be tolerated if unknown
    WALLET_FLAG_KEY_ORIGIN_METADATA = (1ULL << 1),

    // wallet flags in the upper section (> 1 << 31) will prevent opening if unknown
    WALLET_FLAG_DISABLE_PRIVATE_KEYS = (1ULL << 32),
    // wallet flag to indicate a blank wallet (not even seed is created)
    WALLET_FLAG_BLANK_WALLET         = (1ULL << 33),
    // wallet flag to enable descriptor-based wallet (implies SQLite usually)
    WALLET_FLAG_DESCRIPTORS          = (1ULL << 34),
};

static constexpr uint64_t g_known_wallet_flags = WALLET_FLAG_DISABLE_PRIVATE_KEYS | WALLET_FLAG_BLANK_WALLET | WALLET_FLAG_DESCRIPTORS;

// ─── Key pool entry ──────────────────────────────────────────────────────────

/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;
    bool fInternal;    // for change outputs
    bool m_pre_split;  // for keys generated before keypool split upgrade

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn, bool internalIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (ser_action.ForRead()) {
            try { READWRITE(fInternal); }
            catch (std::ios_base::failure&) { fInternal = false; }
            try { READWRITE(m_pre_split); }
            catch (std::ios_base::failure&) { m_pre_split = false; }
        } else {
            READWRITE(fInternal);
            READWRITE(m_pre_split);
        }
    }
};

#endif // BITCOIN_WALLET_WALLETUTIL_H
