# Cutting XPChain Core v0.27.0

After merging the release-prep PR that sets `_CLIENT_VERSION_IS_RELEASE` to
`true`, create the release from `master`:

```bash
git checkout master
git pull origin master
git tag -a v0.27.0 -m "XPChain Core v0.27.0"
git push origin v0.27.0
```

Or run **Actions → Build and Release XPChain Core → Run workflow** with tag
`v0.27.0`.

## What the workflow publishes

- `xpchain-v0.27.0-linux-x86_64.tar.gz`
- `xpchain-v0.27.0-win64.zip`
- `xpchain-v0.27.0-win32.zip`
- `xpchain-v0.27.0-macos.tar.gz`
- `xpchain-v0.27.0-macos.dmg` (when GUI packaging succeeds)

Publish waits for **linux + windows64 + windows32 + macos**.

## After artifacts are up

1. Attach SHA-256 checksums in the GitHub Release notes (or verify workflow output).
2. Complete remaining boxes in `doc/release-validation-v0.27.0.md`.
3. Point operators at `doc/exchange-integration.md` and `doc/wallet-upgrade.md`.
