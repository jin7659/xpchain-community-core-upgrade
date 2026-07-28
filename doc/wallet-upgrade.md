# XPChain Wallet Upgrade Guide

This document describes how to upgrade XPChain Core wallet releases safely, with emphasis on BIP39/BIP44 mnemonic recovery and web wallet compatibility.

## Supported Versions

| Source | Target | Notes |
|--------|--------|-------|
| 0.17.0-2 / 0.17.0-3 / 0.17.0-4 | 0.27.0+ | Recommended upgrade path |
| Legacy BDB `wallet.dat` | Same release | Use `-upgradewallet` for feature version bump |
| Web wallet mnemonic | Desktop `xpchain-qt` / `xpchain-cli` | BIP44 with `coin_type=0` |

## Before You Upgrade

1. **Back up your wallet** using `backupwallet` or copy the entire data directory.
2. **Record your mnemonic** if you use the web wallet or plan to recover on another device.
3. **Close XPChain Core** before replacing binaries or copying wallet files.
4. **Note your coin_type** if you imported from the web wallet (see [BIP44 coin_type](#bip44-coin_type) below).

Automatic backup on GUI mnemonic import is stored under:

```
<datadir>/wallet_backups/backup_before_mnemonic_YYYYMMDD_hhmmss.dat
```

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

### GUI (xpchain-qt)

1. For a **clean recovery**, create a **new empty wallet** first (`File → Create Wallet`).
2. Open **Import Mnemonic** from the menu.
3. Enter your mnemonic words.
4. Enable **BIP44 (web wallet compatible)** when recovering from the XPChain web wallet.
5. Choose the correct **coin type**:
   - **coin_type 0** — current web wallet (default since 2026)
   - **coin_type 398** — legacy web wallet path (older deployments only)
6. Confirm import. The wallet backs up automatically before importing keys.
7. Wait for the blockchain rescan to finish before checking your balance.

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

## Encrypted Wallets

- Unlock the wallet before mnemonic import (GUI prompts automatically; CLI requires `walletpassphrase`).
- After upgrading binaries, encrypted wallets open normally with your existing passphrase.
- SQLite wallets with SQLCipher require a build that includes SQLCipher support.

## Exchange / Hot Wallet Operators

When running a custodial or exchange wallet:

- Start `xpchaind` with **`-minting=0`** to disable staking on customer deposits.
- Understand **immature rewards** (~100 blocks) and **coinstake** transactions — see the main [README](../README.md) exchange section.
- Use **native Segwit (bech32)** addresses for deposits and withdrawals.

## Wallet File Locations

| File / directory | Purpose |
|------------------|---------|
| `wallet.dat` | Legacy default wallet (BDB) |
| `wallets/` | Multi-wallet directory (BDB or SQLite per wallet) |
| `wallet_backups/` | Automatic pre-mnemonic-import backups (GUI) |
| `backups/` | Non-legacy wallet backup directory |

See [files.md](files.md) for the full data directory layout.

## Regression Tests

The following functional tests cover mnemonic recovery behaviour:

- `test/functional/wallet_mnemonic_bip44.py` — BIP44 import, coin_type 0 vs 398
- `test/functional/wallet_mnemonic_rescan.py` — rescan and balance recovery

Run them after building:

```bash
test/functional/wallet_mnemonic_bip44.py
test/functional/wallet_mnemonic_rescan.py
```

## Getting Help

If recovery still fails after following this guide:

1. Do **not** delete wallet files or mnemonics.
2. Preserve `wallet_backups/` copies.
3. Note your XPChain Core version (`xpchain-cli --version`), coin_type used, and whether the wallet is encrypted.
4. Report issues to the [xpchain-community-core-upgrade](https://github.com/jin7659/xpchain-community-core-upgrade) repository with logs from `debug.log` (redact mnemonics and passphrases).
