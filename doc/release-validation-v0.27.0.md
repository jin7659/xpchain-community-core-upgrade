# Release validation checklist — XPChain Core v0.27.0

Status: **partially validated — formal release deferred** (Linux CI + wallet suite green; no `v0.27.0` tag planned for now)

- Repository: `jinseob-dev/xpchain-community-core-upgrade`
- Version in tree: `0.27.0` (`configure.ac`)
- `_CLIENT_VERSION_IS_RELEASE`: `false` (keep pre-release warning until a formal tag is intentionally cut)
- Validation merged via [PR #13](https://github.com/jinseob-dev/xpchain-community-core-upgrade/pull/13)
- Release-prep PR #14 was closed without merge (release cancelled)

## Scope vs 0.17.0-4

Prior packaging-line validation lives in
[`release-validation-v0.17.0-4.md`](release-validation-v0.17.0-4.md).
0.27.0 additionally needs wallet/storage and CI verification below.

## Build matrix (expected artifacts)

| Platform | Artifact | Validated |
|----------|----------|-----------|
| Linux x86_64 | `xpchain-v0.27.0-linux-x86_64.tar.gz` | [ ] pending tagged release workflow |
| Windows x64 | `xpchain-v0.27.0-win64.zip` | [ ] pending tagged release workflow |
| Windows x86 | `xpchain-v0.27.0-win32.zip` | [ ] pending tagged release workflow |
| macOS | `xpchain-v0.27.0-macos.tar.gz` | [ ] pending tagged release workflow |
| macOS | `xpchain-v0.27.0-macos.dmg` | [ ] pending tagged release workflow |

Confirm the GitHub Release publish job waited for **linux + windows64 + windows32 + macos** (fixed in PR #11).

Draft-only asset today: `XPChain-Core.dmg` on the untagged draft release (not a full matrix).

## CI gates

- [x] `CI` workflow `linux-unit-tests` green on the validation commit
- [x] `CI` workflow `linux-functional-tests` green (real wallet suite; harness false-green fixed)
- [ ] Manual smoke: `xpchaind -version` / `xpchain-qt -version` on each release artifact

### Functional suite covered (CI)

- `wallet_sqlite_encryption.py`
- `wallet_sqlite_default.py`
- `wallet_migrate_sqlite.py`
- `wallet_mnemonic_bip44.py`
- `wallet_mnemonic_rescan.py`
- `wallet_encryption.py`
- `wallet_backup.py`

`wallet_multiwallet.py` is temporarily excluded: `GetWalletDir` auto-creates `wallets/` and breaks that test’s datadir-layout assumptions.

## Wallet upgrade checks

- [x] Existing BDB wallet opens without migration (`wallet_sqlite_default`)
- [x] New wallet is created as SQLite by default (`wallet_sqlite_default`)
- [x] `migratewallet` produces a usable SQLite wallet (`wallet_migrate_sqlite`)
- [x] SQLCipher wallet opens with `-walletdbpassphrase` / `loadwallet` passphrase (`wallet_sqlite_encryption`)
- [x] `walletpassphrasechange` rekeys SQLCipher successfully (`wallet_sqlite_encryption`)
- [x] BIP44 mnemonic import recovers expected addresses (`wallet_mnemonic_bip44` / `wallet_mnemonic_rescan`)

## Bugs fixed during validation

- Functional harness never set `ENABLE_UTILS` (`@BUILD_BITCOIN_UTILS_TRUE@` leftover) → false green.
- `IsSqlcipherEncryptedFile` treated BDB files as encrypted SQLite.
- SQLite `Backup` allowed overwriting the source wallet path.
- `BerkeleyEnvironment::Flush(true)` erased `g_dbenvs` while `Flush` was still running (UAF / SIGBUS/SIGSEGV on BDB shutdown); `Close()` now resets `DbEnv` for safe reopen.

## Operator / exchange checks

- [ ] `-minting=0` for exchange hot wallets (documented; not re-run here)
- [ ] Amount precision (4 decimal places) and bech32 deposit addresses verified
- [ ] Review `doc/exchange-integration.md` against the deployed binary

## Sign-off

| Role | Name | Date | Notes |
|------|------|------|-------|
| Builder | cloud-agent | 2026-08-15 | PR #13 CI green; local wallet suite green |
| Tester | | | Cross-OS artifacts + exchange smoke still open |
| Releaser | | | Formal `v0.27.0` tag deferred; leave `IS_RELEASE=false` |
