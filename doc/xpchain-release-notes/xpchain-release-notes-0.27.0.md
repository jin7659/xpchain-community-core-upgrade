XPChain Core version 0.27.0 (pre-release)

  <https://github.com/jinseob-dev/xpchain-community-core-upgrade>

This is a community upgrade release relative to the 0.17.0-4 packaging line.
It focuses on wallet storage modernization (SQLite / SQLCipher), mnemonic
recovery, scheduled Taproot support, CI coverage, and project metadata
updates for the `xpchain-community-core-upgrade` repository.

`_CLIENT_VERSION_IS_RELEASE` is currently `false` until a formal tagged release
is published from this repository. Set it to `true` when cutting the official
`v0.27.0` tag.

Please report bugs using the issue tracker:

  <https://github.com/jinseob-dev/xpchain-community-core-upgrade/issues>

How to Upgrade
==============

If you are running an older version, shut it down and wait until it has
completely exited. Then install the binaries from this release and restart.

Existing Berkeley DB wallets continue to open. New wallets default to SQLite.
Encrypted-at-rest SQLite wallets use SQLCipher when built with SQLCipher
support. See `doc/wallet-upgrade.md` and `doc/exchange-integration.md`.

Compatibility
=============

XPChain Core 0.27.0 is intended to provide release artifacts for:

- Linux x86_64 (tar.gz)
- macOS (dmg, tar.gz)
- Windows x86 / x64 (zip)

Build and packaging are driven by `.github/workflows/release.yml`. The publish
job waits for Linux, Windows, and macOS artifacts before creating the GitHub
Release.

Notable changes
===============

Wallet: SQLite default and SQLCipher at-rest encryption
------------------------------------------------------

- New wallets default to SQLite; existing BDB wallets remain supported.
- SQLCipher is integrated via depends for encrypted-at-rest wallet databases.
- `-walletdbpassphrase` UX improved for GUI and configuration.
- `loadwallet` accepts an optional db passphrase for SQLCipher wallets.
- `walletpassphrasechange` coverage includes SQLCipher rekey behavior.
- `migratewallet` RPC migrates Berkeley DB wallets to SQLite.

Wallet: mnemonic recovery and safety
------------------------------------

- BIP44 mnemonic recovery improvements and web-wallet compatibility fixes.
- Rescan safety and automatic backup path fixes.
- Regression tests for mnemonic recovery and SQLite wallet behavior.

Consensus / protocol-related wallet features
--------------------------------------------

- Taproot (BIP341, BIP342) validation support is included, but Taproot is
  **not active yet**. Mainnet activates at block 4,200,000; testnet and
  regtest are active from genesis. Until the activation height, a node
  without Taproot support accepts any spend of a witness v1 output, so coins
  held at a bech32m address are not protected by consensus.
- Accordingly the wallet no longer defaults to bech32m. `getnewaddress` and
  `getrawchangeaddress` reject an explicitly requested `bech32m` type before
  activation, `-addresstype=bech32m` / `-changetype=bech32m` fall back to
  `bech32` and warn at startup, and the GUI hides the Bech32m option. If you
  already hold coins at a bech32m address, move them to a bech32 address.
- The coinstake script flags now come from the height of the block being
  validated rather than from the active tip, so a block is judged the same
  during a reorg or a `-checklevel=4` rescan as it is when first connected.
- Mining reward scaling divisor corrected (10^8 → 10^4) where applicable.

Build, CI, and packaging
------------------------

- C++17 toolchain requirement.
- GitHub Actions CI: unit tests, Qt tests, and wallet functional tests with
  SQLCipher coverage.
- Release workflow publish step now depends on macOS as well as Linux/Windows
  so macOS `.dmg` / `.tar.gz` artifacts are included.

Documentation and project metadata
----------------------------------

- Exchange integration guide (`doc/exchange-integration.md`).
- Wallet upgrade guide (`doc/wallet-upgrade.md`).
- Repository URLs, CI badge, and source links updated to
  `jinseob-dev/xpchain-community-core-upgrade`.

Upgrade notes for operators
===========================

- Exchanges should review `doc/exchange-integration.md` before upgrading
  deposit/withdrawal wallets, especially SQLCipher passphrase handling and
  minting disabled (`-minting=0`) requirements.
- After upgrading, create a fresh wallet backup before migrating BDB → SQLite.
- Do not commit mnemonics or wallet passphrases to issue reports or logs.

XPChain 0.27.0 change log (summary)
-----------------------------------

- SQLite default wallets + BDB compatibility
- SQLCipher at-rest encryption and passphrase UX/RPC
- `migratewallet` and mnemonic recovery improvements
- CI functional coverage for wallet encryption/migration/mnemonic paths
- Release publish waits for macOS artifacts
- Repository / documentation URL hygiene for the community upgrade fork
