# Exchange listing toolkit

Helpers that make XPChain (XPC) exchange onboarding faster: generate listing
fact sheets, verify a hot-wallet node, and demonstrate correct deposit
filtering (4 decimals, long bech32 addresses, no coinstake false credits).

Full prose guide: [`doc/exchange-integration.md`](../../doc/exchange-integration.md).

## Tools

| Script | Purpose |
|--------|---------|
| `listing_facts.py` | Emit a Markdown/JSON fact sheet for exchange listing forms |
| `readiness_check.py` | Probe a running `xpchaind` for exchange hot-wallet readiness |
| `sample_deposit_monitor.py` | Reference deposit poller with safe category filters |
| `self_test.py` | Offline unit tests (no node required) |
| `xpc_facts.py` / `rpc.py` | Shared constants and JSON-RPC client |

All scripts are **Python 3 stdlib only**.

## Quick start

```bash
# Offline fact sheet (attach to listing applications)
./contrib/exchange/listing_facts.py --format md > /tmp/xpc-listing.md
./contrib/exchange/listing_facts.py --format json --chain main

# Optional: enrich with a live node
./contrib/exchange/listing_facts.py --datadir ~/.xpchain --live --format md

# Readiness check against a hot wallet (must use -minting=0)
./contrib/exchange/readiness_check.py --datadir ~/.xpchain
./contrib/exchange/readiness_check.py --rpcuser u --rpcpassword p --rpcport 18999 --chain regtest --json

# Reference deposit monitor
echo 'xpc1q...' > /tmp/deposits.txt
./contrib/exchange/sample_deposit_monitor.py --datadir ~/.xpchain --address-file /tmp/deposits.txt --minconf 6 --once

# Offline tests
python3 contrib/exchange/self_test.py
```

## What exchanges usually need (covered here)

1. **Decimals = 4** (not Bitcoin’s 8) — fact sheet + readiness check.
2. **Bech32 HRP** `xpc` / `txpc` / `xpcrt` and long addresses — fact sheet + address checks.
3. **`-minting=0` (verify via `getmininginfo.minting == false`)** on hot wallets — readiness heuristic + docs.
4. **Immature (~100 blocks)** must not be credited as available — monitor filters + docs.
5. **RPC map** for deposit / withdraw / validate — fact sheet `required_rpcs`.

## Regtest smoke recipe

```bash
D=/tmp/xpc-exchange-smoke
mkdir -p "$D"
printf 'regtest=1\nserver=1\nminting=0\nrpcuser=u\nrpcpassword=p\n[regtest]\nrpcport=18999\n' > "$D/xpchain.conf"
./src/xpchaind -datadir="$D" -daemon
sleep 2
./contrib/exchange/readiness_check.py --rpcuser u --rpcpassword p --rpcport 18999 --chain regtest
./contrib/exchange/listing_facts.py --rpcuser u --rpcpassword p --rpcport 18999 --chain regtest --live --format md
./src/xpchain-cli -datadir="$D" stop
```

## Korean summary / 한국어 요약

거래소 상장 제출용 **기술 스펙 시트**(`listing_facts.py`), 핫월렛 노드 **점검**(`readiness_check.py`),
입금 감지 **참고 구현**(`sample_deposit_monitor.py`)을 제공합니다. 핵심 주의사항은
소수점 4자리, bech32 HRP(`xpc`), `-minting=0`, immature 미입금 처리입니다.
