# Release validation checklist — XPChain Core v0.27.0

Status: **pending formal validation** (pre-release / test builds only so far)

- Repository: `jinseob-dev/xpchain-community-core-upgrade`
- Version in tree: `0.27.0` (`configure.ac`)
- `_CLIENT_VERSION_IS_RELEASE`: `false` until this checklist is completed and a `v0.27.0` tag is published

## Scope vs 0.17.0-4

Prior packaging-line validation lives in
[`release-validation-v0.17.0-4.md`](release-validation-v0.17.0-4.md).
0.27.0 additionally needs wallet/storage and CI verification below.

## Build matrix (expected artifacts)

| Platform | Artifact | Validated |
|----------|----------|-----------|
| Linux x86_64 | `xpchain-v0.27.0-linux-x86_64.tar.gz` | [ ] |
| Windows x64 | `xpchain-v0.27.0-win64.zip` | [ ] |
| Windows x86 | `xpchain-v0.27.0-win32.zip` | [ ] |
| macOS | `xpchain-v0.27.0-macos.tar.gz` | [ ] |
| macOS | `xpchain-v0.27.0-macos.dmg` | [ ] |

Confirm the GitHub Release publish job waited for **linux + windows64 + windows32 + macos**.

## CI gates

- [ ] `CI` workflow `linux-unit-tests` green on the release commit
- [ ] `CI` workflow `linux-functional-tests` green (SQLite / SQLCipher / mnemonic suite)
- [ ] Manual smoke: `xpchaind -version` / `xpchain-qt -version` on each artifact

## Wallet upgrade checks

- [ ] Existing BDB wallet opens without migration
- [ ] New wallet is created as SQLite by default
- [ ] `migratewallet` produces a usable SQLite wallet (backup first)
- [ ] SQLCipher wallet opens with `-walletdbpassphrase` / GUI prompt / `loadwallet` passphrase
- [ ] `walletpassphrasechange` rekeys SQLCipher successfully
- [ ] BIP44 mnemonic import recovers expected addresses (see `doc/wallet-upgrade.md`)

## Operator / exchange checks

- [ ] `-minting=0` for exchange hot wallets
- [ ] Amount precision (4 decimal places) and bech32 deposit addresses verified
- [ ] Review `doc/exchange-integration.md` against the deployed binary

## Sign-off

| Role | Name | Date | Notes |
|------|------|------|-------|
| Builder | | | |
| Tester | | | |
| Releaser | | | Set `_CLIENT_VERSION_IS_RELEASE` to `true`, tag `v0.27.0` |
