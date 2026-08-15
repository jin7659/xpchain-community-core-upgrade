# XPChain Core

XPChain Core (XPC) is a C++ cryptocurrency based on Bitcoin 0.17.0 with a proof-of-stake
consensus model. The reference software is built with autotools and produces the node
daemon `xpchaind`, the RPC client `xpchain-cli`, the tx tool `xpchain-tx`, and (optionally)
the `xpchain-qt` GUI. Dependencies are compiled from source via the Bitcoin-style
`depends/` system. General build docs live in `doc/build-unix.md`; the authoritative
build/test recipe is `.github/workflows/ci.yml`.

## Cursor Cloud specific instructions

This section captures durable, non-obvious context for working in the cloud environment.
Standard commands are in `doc/build-unix.md`, `README.md`, and `.github/workflows/ci.yml`;
only the caveats below are XPChain/cloud specific.

### Scope of the prepared environment

The environment is set up **headless** (`NO_QT=1`, `--with-gui=no`): the daemon, wallet
(BerkeleyDB + SQLCipher-encrypted SQLite), CLI, tx tool, unit tests, and Python functional
tests. This is the core product and matches CI's `linux-functional-tests` job. The Qt GUI
(`--with-gui=qt5`, CI's `linux-unit-tests` job) is **not** built here because compiling Qt
from `depends` is very expensive; build it separately if you need the GUI.

### Dependencies are prebuilt into the snapshot

- System build packages are installed in the snapshot: `build-essential libtool
  autotools-dev automake autoconf pkg-config bsdmainutils curl git python3 bison tcl`.
  Note `tcl` is required to build `sqlcipher` (generates its sources with `tclsh`) even
  though CI does not list it — CI only passes because it restores a `depends/built` cache.
- The `depends/` tree is already compiled and cached (`depends/built/` +
  `depends/x86_64-linux-gnu/`) and baked into the snapshot. The startup/update script
  re-runs `cd depends && make HOST=x86_64-linux-gnu NO_QT=1 ...`, which is a no-op when the
  cache is present.

### Known gotcha: libevent vs glibc >= 2.36 (Ubuntu 24.04)

This VM is Ubuntu 24.04 (glibc 2.39); CI is Ubuntu 22.04 (glibc 2.35). The pinned
`libevent 2.1.8-stable` unconditionally defines `arc4random_buf`, which collides with
glibc >= 2.36's own declaration (`error: static declaration of 'arc4random_buf' follows
non-static declaration`), and with the default `arc4random` detection it also fails to link
(`undefined reference to arc4random_addrandom`). The prebuilt `libevent` in the snapshot was
compiled with a compatibility fix. If the `depends/built` cache is ever wiped and libevent
must be rebuilt from scratch on glibc >= 2.36, rebuild it with the bundled RNG forced on:

```
export ac_cv_func_arc4random=no ac_cv_func_arc4random_addrandom=no
```

and guard libevent's `arc4random_buf` definition in `arc4random.c` with
`#if !defined(__GLIBC_PREREQ) || !__GLIBC_PREREQ(2, 36)` so it defers to glibc's when
present. (Building inside an Ubuntu 22.04 container avoids the issue entirely.)

### Building

```
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-linux-gnu/share/config.site ./configure --prefix=/ --with-gui=no
make -j"$(nproc)"
```

`make` (not `make check`) also builds the unit-test binary `src/test/test_xpchain`. The
startup/update script does **not** run `make`; rebuild manually after pulling code changes.

### Testing

- Unit tests: `./src/test/test_xpchain` (302 cases).
- Functional tests: `python3 test/functional/test_runner.py <tests...> --jobs 2`.

Two functional-harness caveats (both are why CI's functional job is currently a silent
no-op / false green):

1. `test/config.ini.in` still references the pre-rebrand automake conditional
   `@BUILD_BITCOIN_UTILS_TRUE@` (the real one is `BUILD_XPCHAIN_UTILS`). As a result the
   generated `test/config.ini` never sets `ENABLE_UTILS`, so `test_runner.py` prints
   "No functional tests to run" and exits 0 without running anything. Workaround: in the
   generated (gitignored) `test/config.ini`, replace the
   `@BUILD_BITCOIN_UTILS_TRUE@ENABLE_UTILS=true` line with `ENABLE_UTILS=true`. Re-running
   `./configure` regenerates the broken file, so re-apply after configuring.
2. This 0.17-era framework does **not** support `--timeout-factor` (`create_cache.py` rejects
   it); omit that flag even though CI passes it.

With the harness fixed, the SQLCipher suite `wallet_sqlite_encryption.py` passes. Several
other `wallet_*` functional tests (`wallet_backup`, `wallet_encryption`,
`wallet_migrate_sqlite`, `wallet_multiwallet`, `wallet_sqlite_default`) currently fail due
to pre-existing code/test mismatches unrelated to environment setup (CI never actually
executed them, per caveat 1).

### Running the node (regtest example)

```
D=/tmp/xpc-regtest; mkdir -p $D
printf 'regtest=1\nserver=1\nminting=0\nrpcuser=u\nrpcpassword=p\n[regtest]\nrpcport=18999\n' > $D/xpchain.conf
./src/xpchaind -datadir=$D -daemon
./src/xpchain-cli -datadir=$D getblockchaininfo
```

Note: the regtest P2P port is `28798`; two regtest daemons on one machine collide on it, so
give extra nodes `-listen=0` or a distinct `-port`. Use `minting=0` to disable PoS minting.
SQLCipher-encrypted SQLite wallets need `-walletdbpassphrase=<pass>` (or `loadwallet` with a
`dbpassphrase`) to open after `encryptwallet`.
