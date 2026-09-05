# XPChain Wallet Upgrade Guide

This document describes how to upgrade XPChain Core wallet releases safely, with emphasis on BIP39/BIP44 mnemonic recovery and web wallet compatibility.

## Supported Versions

| Source | Target | Notes |
|--------|--------|-------|
| 0.17.0-2 / 0.17.0-3 / 0.17.0-4 | 0.27.0+ | Recommended upgrade path |
| Legacy BDB `wallet.dat` | Same release | Deprecated in transitional phase; migrate to SQLite via `migratewallet` |
| Web wallet mnemonic | Desktop `xpchain-qt` / `xpchain-cli` | BIP44 with `coin_type=0` |

### Transitional Phase Policy (Berkeley DB → SQLite)

In this release cycle, XPChain Core is in a **transitional phase**:
* **New wallets** default to SQLite (encrypted at rest with SQLCipher if password protected).
* **Legacy Berkeley DB wallets** remain fully supported and readable to prevent asset loss.
* Loading a BDB wallet prints an informational recommendation to migrate.
* Creating new BDB wallets (`createwallet ... force_berkeley=true`) issues a deprecation warning and will be phased out in future releases.
* Users can migrate to SQLite at any time via **File → Migrate Wallet to SQLite…** in the GUI or `migratewallet` via RPC.

## Before You Upgrade

1. **Back up your wallet** using `backupwallet` or copy the entire data directory.
2. **Record your mnemonic** if you use the web wallet or plan to recover on another device.
3. **Close XPChain Core** before replacing binaries or copying wallet files.
4. **Note your coin_type** if you imported from the web wallet (see [BIP44 coin_type](#bip44-coin_type) below).

Automatic backup on GUI mnemonic import is stored under:

```
<datadir>/wallet_backups/backup_before_mnemonic_YYYYMMDD_hhmmss.dat   # legacy BDB wallets
<datadir>/backups/backup_before_mnemonic_YYYYMMDD_hhmmss.dat          # non-legacy / descriptor wallets
```

The restore dialog shows the path that applies to the current wallet.

## Standard Upgrade Steps

1. Install the new XPChain Core binaries (`xpchaind`, `xpchain-qt`, `xpchain-cli`).
2. Start the node normally. The wallet opens existing `wallet.dat` or SQLite wallets in `<datadir>/wallets/`.
3. Optional: run a one-time wallet feature upgrade:

```bash
xpchain-cli -upgradewallet
```

Or with an explicit maximum feature version:

```bash
xpchaind -upgradewallet=169900
```

4. Verify balance and recent transactions in the GUI or via RPC:

```bash
xpchain-cli getwalletinfo
xpchain-cli getbalance
```

## BIP39 / BIP44 Mnemonic Recovery

XPChain supports restoring a wallet from a 12- or 24-word English BIP39 mnemonic.

### Generate & backup a new mnemonic (GUI)

Existing wallets generally **cannot export** a BIP39 phrase unless they were created from one. To create a new mnemonic-backed wallet:

1. Open **File → Set Up Wallet** and choose **Generate & backup mnemonic** (or **File → Advanced → Generate & Backup Mnemonic**).
2. Choose wallet name, 12 or 24 words, BIP44 (recommended for web wallet address compatibility), and optional encryption.
3. Write down the numbered word list offline, then confirm you stored it safely.
4. Re-enter the phrase to confirm the backup.
5. The GUI creates a legacy HD wallet, unlocks it if encrypted, and imports the mnemonic (`importmnemonic`, no full rescan for a fresh wallet).

Keep the written phrase offline. It is not recoverable from the wallet database later.

### Restore an existing mnemonic (GUI)

1. For a **clean recovery**, create a **new empty wallet** first (**File → Set Up Wallet → Create empty wallet**, or **File → Advanced → Create Wallet**), or use **Create empty wallet first…** in the restore dialog.
2. Open **File → Set Up Wallet → Restore from mnemonic** (or **File → Advanced → Restore Wallet from Mnemonic**).
3. Enter your mnemonic words (word count and BIP39 list hints appear as you type).
4. Leave **Recover from XPChain web wallet (BIP44)** checked when recovering from the XPChain web wallet.
5. Choose the correct **coin type**:
   - **coin_type 0** (default) — current web wallet
   - **Legacy coin_type 398** — only if coin_type 0 finds nothing (older deployments)
6. Confirm import. The wallet backs up automatically before importing keys.
7. Wait for the blockchain rescan progress dialog to finish (a notification appears when it completes) before checking your balance.

### CLI (xpchain-cli)

```bash
xpchain-cli importmnemonic "word1 word2 ... word12" \
  '{"bip44":true,"bip44_coin_type":0,"gap_limit":1000,"rescan":true}'
```

Options:

| Field | Default | Description |
|-------|---------|-------------|
| `passphrase` | `""` | BIP39 optional passphrase |
| `bip44` | `false` | Use BIP44 derivation (`m/44'/coin_type'/0'/…`) |
| `bip44_coin_type` | `0` | `0` = current web wallet, `398` = legacy |
| `gap_limit` | `1000` | Addresses derived per external/internal chain |
| `rescan` | `true` | Rescan blockchain after import |

If the wallet is encrypted, unlock first:

```bash
xpchain-cli walletpassphrase "your_passphrase" 600
```

## BIP44 coin_type

The derivation path is `m/44'/<coin_type>'/0'/change/index`.

| coin_type | Use when |
|-----------|----------|
| **0** | Recovering from the **current** XPChain web wallet |
| **398** | Recovering from an **older** web wallet that used the legacy path |

Using the wrong coin_type produces **different addresses**. Your coins will not appear until you import with the correct coin_type and rescan.

To verify the path of an imported address:

```bash
xpchain-cli getaddressinfo <address>
```

Look for `"hdkeypath": "m/44'/0'/0'/0/0"` (example for coin_type 0, first external address).

## Balance Shows Zero After Import

Work through this checklist before assuming funds are lost:

1. **Rescan still running** — wait for sync/rescan to complete (GUI overlay or `getwalletinfo`).
2. **Wrong coin_type** — retry import with `bip44_coin_type` 0 or 398 as appropriate.
3. **Wrong mnemonic or passphrase** — BIP39 passphrase changes the derived seed entirely.
4. **Imported into existing wallet** — mnemonic import **merges** keys; for clean recovery, use a new empty wallet.
5. **Immature staking rewards** — PoS rewards need ~100 blocks before spendable; they may not count toward spendable balance immediately.
6. **Node not synced** — ensure the chain tip is up to date (`getblockchaininfo`).

Manual rescan:

```bash
xpchain-cli rescanblockchain 0
```

## Migrating Berkeley DB to SQLite

**New wallets use SQLite by default.** Existing Berkeley DB wallets continue to open normally; nothing is converted automatically.

Legacy BDB wallets can be migrated with `migratewallet`:

### GUI (xpchain-qt)

1. Open the Berkeley DB wallet you want to migrate.
2. Choose **File → Migrate Wallet to SQLite…**
3. Confirm the destination (default `<name>.sqlite`), backup, and whether to switch to the new wallet.
4. After success, verify balance on Overview and send a small test transaction.

Migration is disabled when the current wallet is already SQLite.

### CLI

```bash
# On the wallet to migrate (use -rpcwallet=<name> if not default)
xpchain-cli migratewallet '{"backup":true,"load_new":true}'
```

Options:

| Field | Default | Description |
|-------|---------|-------------|
| `destination` | `<name>.sqlite` | Output SQLite wallet path (must end with `.sqlite`) |
| `backup` | `true` | Copy source wallet to `<source>.pre_migrate_<timestamp>.bak` |
| `load_new` | `false` | Unload BDB wallet and load the migrated SQLite wallet |

Example with explicit destination:

```bash
xpchain-cli -rpcwallet=legacy_bdb migratewallet \
  '{"destination":"legacy_bdb.sqlite","backup":true,"load_new":true}'
```

To create a legacy Berkeley DB wallet intentionally (for testing or special cases):

```bash
xpchain-cli createwallet "legacy_bdb" false false "" null true
# arguments: name, disable_private_keys, descriptors, passphrase, load_on_startup, berkeley
```

After migration **without** `load_new`:

```bash
xpchain-cli unloadwallet "legacy_bdb"
xpchain-cli loadwallet "legacy_bdb.sqlite"
```

### Notes

- The original BDB file is **not deleted** — only copied to SQLite.
- Encrypted wallets migrate correctly (keys remain encrypted at the application layer).
- Descriptor wallets are already SQLite; `migratewallet` rejects non-BDB sources.
- Always verify balance and a test transaction after migration.

## Encrypted Wallets

- **GUI Create Wallet** can encrypt on create (recommended). The same passphrase protects spending keys and, on SQLCipher builds, the wallet file at rest.
- CLI: `xpchain-cli createwallet "name" false true "passphrase"` (name, disable_private_keys, descriptors, passphrase).
- Unlock the wallet before mnemonic import (GUI prompts automatically; CLI requires `walletpassphrase`).
- After upgrading binaries, encrypted wallets open normally with your existing passphrase.
- SQLite wallets with SQLCipher require a build that includes SQLCipher support.

## Exchange / Hot Wallet Operators

When running a custodial or exchange wallet:

- Start `xpchaind` with **`-minting=0`** to disable staking on customer deposits.
- Understand **immature rewards** (~100 blocks) and **coinstake** transactions — see the main [README](../README.md) exchange section.
- Use **native Segwit (bech32)** addresses for deposits and withdrawals.

For a full integration guide (deposit monitoring, decimals, common mistakes), see [exchange-integration.md](exchange-integration.md).

## SQLCipher and Wallet Encryption

Official **depends** builds include **SQLCipher** for SQLite wallet at-rest encryption. When SQLCipher is enabled:

- `encryptwallet` rewrites the SQLite file with a database-level key (in addition to application-layer key encryption).
- `getwalletinfo` reports `"sqlcipher": true|false` (build support), `"databaseformat": "sqlite"|"berkeley"`, and `"encrypted_at_rest": true|false` (this wallet file).
- After restart there are **two separate steps** (same passphrase for both when you used `encryptwallet`):
  1. **Open wallet file** — GUI dialog titled like “Open encrypted wallet file” if `-walletdbpassphrase` is unset (remembered for the process only; not written to disk). Daemon: `-walletdbpassphrase` / `walletdbpassphrase=` in `xpchain.conf`, or `loadwallet "name" "dbpassphrase"`.
  2. **Unlock spending keys** — GUI “Unlock wallet keys” / Settings unlock, or `walletpassphrase`. Opening the file does **not** unlock keys by itself.
- Prefer conf/`-walletdbpassphrase` for unattended daemons; never store the passphrase in Qt settings or shared docs.

If your build shows `"sqlcipher": false`, install `libsqlcipher-dev` (or build via `depends/`) before deploying custodial wallets.

## Wallet status in the GUI

The Overview page shows four chips for the **current** wallet:

| Chip | Meaning |
|------|---------|
| SQLite / Berkeley DB | Database engine. BDB wallets can be converted with **File → Migrate Wallet to SQLite…** |
| SQLCipher / Plain file / BDB file | Whether the *file* is encrypted at rest (SQLCipher). Distinct from spending-key encryption. |
| Descriptor / Legacy HD / Watch-only | Key management type |
| Keys locked / unlocked / unencrypted | Spending-key lock (same as the status-bar padlock) |

The status bar also shows a compact **SQLite** or **BDB** chip. Hover it for the same four facts.

`getwalletinfo` exposes the same data as `databaseformat`, `encrypted_at_rest`, `descriptors`, and `unlocked_until`.

## Wallet File Locations

| File / directory | Purpose |
|------------------|---------|
| `wallets/wallet.dat` | Default wallet (SQLite for new installs; existing BDB files still open) |
| `wallets/<name>/wallet.dat` | Named wallet directory layout (SQLite by default; BDB if created with `berkeley=true` or legacy) |
| `wallets/*.sqlite` | Explicit SQLite wallet files |
| `wallet_backups/` | Automatic pre-mnemonic-import backups (GUI) for **legacy** wallets |
| `backups/` | Automatic pre-mnemonic-import backups (GUI) for **non-legacy** wallets |

See [files.md](files.md) for the full data directory layout.

## Regression Tests

The following functional tests cover mnemonic recovery behaviour:

- `test/functional/wallet_mnemonic_bip44.py` — BIP44 import, coin_type 0 vs 398
- `test/functional/wallet_mnemonic_rescan.py` — rescan and balance recovery
- `test/functional/wallet_migrate_sqlite.py` — BDB to SQLite migration
- `test/functional/wallet_sqlite_default.py` — new wallets default to SQLite; BDB still opens
- `test/functional/wallet_sqlite_encryption.py` — SQLCipher at-rest encryption (when enabled)

Run them after building:

```bash
test/functional/wallet_mnemonic_bip44.py
test/functional/wallet_mnemonic_rescan.py
test/functional/wallet_migrate_sqlite.py
test/functional/wallet_sqlite_default.py
test/functional/wallet_sqlite_encryption.py
```

## Getting Help

If recovery still fails after following this guide:

1. Do **not** delete wallet files or mnemonics.
2. Preserve `wallet_backups/` / `backups/` copies from mnemonic import.
3. Note your XPChain Core version (`xpchain-cli --version`), coin_type used, and whether the wallet is encrypted.
4. Report issues to the [xpchain-community-core-upgrade](https://github.com/jinseob-dev/xpchain-community-core-upgrade) repository with logs from `debug.log` (redact mnemonics and passphrases).
