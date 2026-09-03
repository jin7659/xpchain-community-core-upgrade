# XPChain Core version 0.27.0 Release Notes

XPChain Core version 0.27.0 is now officially available from:

  <https://github.com/jinseob-dev/xpchain-community-core-upgrade/releases/tag/v0.27.0>

This is a major milestone release featuring full Proof-of-Stake (PoS) consensus modularization, an upgraded wallet architecture with SQLite/SQLCipher as the default, a seamless transitional compatibility phase for legacy Berkeley DB wallets, rich Qt GUI convenience enhancements, and a brand-new standalone offline wallet tool (`xpchain-wallet`).

Please report bugs using the issue tracker at GitHub:

  <https://github.com/jinseob-dev/xpchain-community-core-upgrade/issues>

---

## How to Upgrade

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes), then run the new binary or installer.

* **Existing Wallets**: All existing wallets (`wallet.dat` using Berkeley DB) are 100% supported without manual intervention. Your wallet will open, sign, and stake seamlessly.
* **Recommended Migration**: It is strongly recommended to migrate existing Berkeley DB wallets to the modern SQLite format using the one-click banner on the GUI Overview page or the `migratewallet` RPC command.

---

## Compatibility

XPChain Core is extensively tested on multiple operating systems:
* **Linux**: Ubuntu 20.04+, Debian 11+, and other modern distributions.
* **macOS**: macOS 11+ (Intel and Apple Silicon ARM64).
* **Windows**: Windows 10 and newer (64-bit).

---

## Notable Changes

### 1. Modular Proof-of-Stake Consensus Engine (`src/pos/`)
* **Complete Modular Separation**: Consensus logic has been decoupled from monolithic legacy code into 5 dedicated submodules:
  - `height`: PoS activation and height rules.
  - `reward`: Deterministic staking subsidy calculation.
  - `kernel`: Core proof-of-stake kernel hash and target difficulty evaluation.
  - `stake`: Stake transaction creation and validation.
  - `staker`: Background staking thread (`ThreadStakeMinter`).
* **Wallet Decoupling via `IStakeableWallet`**: The staking engine now interfaces through an abstract interface (`pos::IStakeableWallet`), allowing the daemon to be built and run cleanly in headless or wallet-disabled mode (`--disable-wallet`).
* **Pure Verification Function**: Introduced `pos::CheckProofOfStakePure()` allowing isolated, deterministic testing of PoS blocks without global mutable chain state dependencies.

### 2. Transitional Wallet Architecture & SQLite Default
* **SQLite as Default**: Newly created wallets default to modern SQLite database storage, providing enhanced transactional reliability and SQLCipher file-level encryption at rest.
* **Full Berkeley DB Backward Compatibility**: Legacy wallets are detected automatically, and users receive clear, non-intrusive migration guidance (`deprecation_warning` in RPC/logs).
* **Seamless Migration (`migratewallet`)**: Effortlessly convert Berkeley DB wallets to SQLite in seconds while backing up original files.

### 3. Qt GUI UX Enhancements
* **One-Click Migration Banner**: When a legacy Berkeley DB wallet is loaded, the Overview page displays a prominent, clean notification banner:
  > *⚠️ This wallet uses the legacy Berkeley DB format. Migrate to the modern SQLite format for enhanced security and performance. [Migrate Now…]*
  Clicking **[Migrate Now…]** opens the migration dialog immediately. Modern SQLite wallets automatically hide the banner.
* **Clickable Status Bar Format Chip**: The status bar BDB chip now supports direct click interaction to trigger migration.
* **Real-time Staking Status Bar Icon**: Status bar displays a live green (active) or gray (inactive) staking icon with comprehensive tooltips explaining current minting status (e.g., active, locked wallet, insufficient mature coins, synchronizing).

### 4. Standalone Offline Wallet Tool (`xpchain-wallet`)
* **Zero Network Exposure**: Introduced the standalone `xpchain-wallet` executable, allowing operations on wallet files without running `xpchaind` or connecting to the P2P network.
* **Key Commands**:
  - `info`: Displays wallet database format (SQLite/BDB), encryption status, descriptors, HD settings, and keypool metrics.
  - `create`: Creates new wallets offline in milliseconds (SQLite by default, `-format=bdb` supported).
  - `salvage`: Recovers keys and records from corrupted Berkeley DB wallet files.

### 5. Continuous Integration & Testing Harness
* Comprehensive unit test suite with 326 test cases running in CI.
* Eliminated mocktime race conditions on node restarts in functional PoS staking tests.
* Full automated functional testing for SQLite default creation, SQLite migration, and standalone wallet tool operations.

---

## Change Log

* **#31**: `pos, wallet: modularize PoS consensus and introduce IStakeableWallet`
* **#31**: `wallet: add BDB deprecation warning and migration recommendations for transitional phase`
* **#31**: `build: resolve GNU ld mutual dependency between server and wallet archives`
* **#31**: `test: eliminate mocktime race condition on node restarts in feature_pos_staking`
* **#32**: `qt, doc: add BDB one-click migration banner, staking status icon, and PoS architecture docs`
* **#32**: `qt: fix ClickableLabel scope, databaseFormat call, and wallet view resolution`
* **#32**: `qt: make migrateWallet public slot for WalletView connection`
* **#33**: `wallet, tool: implement standalone offline xpchain-wallet CLI tool and test suite`
* **#33**: `validation: guard g_signals internals and blockchain tip in offline tool mode`

---

## Credits

Thanks to everyone who contributed to this release, tested code, reported issues, and participated in the XPChain community upgrade!
