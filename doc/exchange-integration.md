# XPChain Exchange Integration Guide

This document is for exchanges, custodians, and payment processors integrating XPChain (XPC) deposits and withdrawals.

For general wallet upgrades, see [wallet-upgrade.md](wallet-upgrade.md).

## Network Summary

| Property | XPChain (XPC) | Bitcoin (reference) |
|----------|---------------|---------------------|
| Decimals | **4** (1 XPC = 10,000 base units) | 8 |
| Typical address length | **~75 characters** | ~34–62 |
| Recommended address type | **Native Segwit (bech32)** | bech32 |
| Consensus | Proof-of-Stake (PoS) + legacy PoW heritage | Proof-of-Work |
| Immature coinbase/stake outputs | **~100 blocks** before spendable | 100 blocks (coinbase only) |

## Node Configuration for Hot Wallets

Run the exchange deposit wallet with minting disabled:

```bash
xpchaind -minting=0 -wallet=exchange_hot
# Confirm: xpchain-cli getmininginfo | jq .minting   # must be false
```

| Flag | Why |
|------|-----|
| `-minting=0` | Prevents staking customer UTXOs. Confirm with `getmininginfo.minting == false`. Minting generates **coinstake** transactions that can look like deposits. |
| Dedicated wallet | Isolates customer funds from operational/staking wallets. |

Recommended additional settings:

- Use **native Segwit (bech32)** for deposit addresses.
- Keep the node fully synced before crediting deposits.
- Use `-wallet` multi-wallet mode to separate hot/cold wallets.

## Address Handling

### Decimals

XPChain amounts support at most **4 decimal places**. Reject or round user withdrawals that exceed this precision.

```text
Valid:   1.2345 XPC
Invalid: 1.23456 XPC  (too many decimals)
```

When serializing amounts in your database, store integers in base units (1 XPC = 10,000) to avoid floating-point errors.

### Address length and format

- Support addresses up to **at least 80 characters**.
- Accept **bech32** (`xpc...` on mainnet, `txpc...` on testnet, `xpcrt...` on regtest) as the default format.
- Legacy and P2SH-Segwit addresses may still appear from older users; do not hard-code 34-character limits.

### Address validation

Validate addresses with your node's RPC before accepting a user withdrawal:

```bash
xpchain-cli validateaddress "xpc1q..."
```

Or derive deposit addresses only from your own wallet:

```bash
xpchain-cli getnewaddress "" "bech32"
```

## Deposit Monitoring

### What to credit

Credit incoming transactions only when:

1. The transaction pays to a **known deposit address** assigned to the user.
2. The transaction is confirmed to your required depth (exchange policy).
3. The output amount uses valid 4-decimal precision.

### Coinstake (critical)

When a wallet mints a block (if `-minting=1`), it creates:

1. A **coinbase**-like reward output.
2. A **coinstake** transaction (often described as "payment to yourself") that spends mature staking inputs.

If your monitor credits **any** incoming payment to deposit addresses without filtering transaction type, coinstake-related outputs can cause **false double credits**.

**Mitigation:** Only credit `send` / `receive` style payments to assigned deposit addresses. Ignore coinstake category transactions unless you explicitly support staking products.

### Immature rewards (~100 blocks)

PoS staking rewards and some received outputs may be **immature** for approximately **100 blocks** after confirmation. They appear on-chain but are not yet spendable.

Symptoms for exchanges:

- User sees a deposit on a block explorer.
- Your node `listtransactions` shows the credit with `category: immature` or similar.
- Balance may show immature funds separately from spendable balance.

**Do not treat immature outputs as available for withdrawal** until matured.

Check immature balance:

```bash
xpchain-cli getwalletinfo
# immature_balance field
```

### Reorgs

Follow your standard confirmation policy (e.g. 6–20 confirmations). Handle chain reorganizations the same as Bitcoin: decrement credits if a deposit tx leaves the main chain.

## Withdrawals

### Constructing transactions

Use the node's wallet RPC:

```bash
xpchain-cli sendtoaddress "xpc1q..." 12.3456
```

Or batch with `sendmany`.

### Fee estimation

```bash
xpchain-cli estimatesmartfee 6
```

XPChain uses 4-decimal amounts; ensure fee rates and output amounts respect this.

### Change outputs

The wallet handles change internally. Track withdrawals by `txid` and vout, not by assuming one output per withdrawal.

## RPC Quick Reference

| Task | RPC |
|------|-----|
| New deposit address | `getnewaddress` |
| List deposits | `listtransactions`, `listreceivedbyaddress` |
| Balance (spendable) | `getbalance` |
| Immature funds | `getwalletinfo` → `immature_balance` |
| Send withdrawal | `sendtoaddress` |
| Validate address | `validateaddress` |
| Wallet encrypted? | `getwalletinfo` |
| Rescan after restore | `rescanblockchain` |

### ZMQ notifications (optional)

If your build enables ZeroMQ, subscribe to `hashblock` and `rawtx` for faster deposit detection. Verify ZMQ is enabled in your build (`getnetworkinfo` / build flags).

## Wallet Security

| Practice | Detail |
|----------|--------|
| Encrypt hot wallet | `encryptwallet` + `walletpassphrase` for signing only |
| SQLCipher builds | Official depends builds include SQLCipher for SQLite at-rest encryption |
| SQLCipher restart (daemon) | Pass `-walletdbpassphrase` at startup or set `walletdbpassphrase=` in `xpchain.conf` (same passphrase as `encryptwallet`) |
| SQLCipher runtime load | `loadwallet "wallet" "dbpassphrase"` when not loaded at startup (per-wallet passphrase for multi-wallet) |
| SQLCipher restart (GUI) | Dialog “Open encrypted wallet file” when `-walletdbpassphrase` is unset; session-only. Keys stay locked until GUI unlock / `walletpassphrase` |
| Cold storage | Keep majority of funds offline; hot wallet holds operational float only |
| Backup | `backupwallet` before upgrades; test restore on staging |
| No minting on hot wallet | `-minting=0` |

## Common Integration Mistakes

| Mistake | Consequence | Fix |
|---------|-------------|-----|
| `-minting=1` on deposit wallet | Coinstake false deposits | Use `-minting=0` |
| 8-decimal amount handling | Invalid txs or wrong amounts | Use 4 decimals / integer base units |
| Address length limit 34–42 | Valid user addresses rejected | Allow ~75+ chars, bech32 |
| Crediting immature outputs | Withdrawals fail / insolvency risk | Wait for maturity or track separately |
| Monitoring all incoming txs globally | Coinstake / internal txs credited | Filter by assigned deposit addresses |
| No rescan after wallet restore | Missing deposits | `rescanblockchain 0` |

## Testnet Checklist

Before mainnet:

1. [ ] Generate bech32 deposit addresses and receive test coins.
2. [ ] Confirm immature → mature transition after ~100 blocks.
3. [ ] Verify coinstake does not create false deposits with `-minting=0`.
4. [ ] Test withdrawal precision at 4 decimals (edge: 0.0001 XPC).
5. [ ] Test wallet backup, restore, and `rescanblockchain`.
6. [ ] Load-test RPC deposit polling or ZMQ under expected volume.

## Listing / onboarding toolkit

Use [`contrib/exchange/`](../contrib/exchange/) to speed up exchange listing paperwork and integration QA:

| Tool | Use |
|------|-----|
| `listing_facts.py` | Markdown/JSON fact sheet for listing forms |
| `readiness_check.py` | Hot-wallet node readiness probe |
| `sample_deposit_monitor.py` | Reference deposit poller (filters immature/coinstake) |
| `listing_application_template.md` | Copy-paste listing application tables |

```bash
./contrib/exchange/listing_facts.py --format md
./contrib/exchange/readiness_check.py --datadir ~/.xpchain
python3 contrib/exchange/self_test.py
```

## Support and References

- Project README exchange section: [../README.md](../README.md)
- Exchange toolkit: [../contrib/exchange/README.md](../contrib/exchange/README.md)
- Wallet upgrade / mnemonic recovery: [wallet-upgrade.md](wallet-upgrade.md)
- Issue tracker: [xpchain-community-core-upgrade](https://github.com/jinseob-dev/xpchain-community-core-upgrade)

When reporting integration issues, include: XPChain Core version (`xpchain-cli --version`), network (mainnet/testnet/regtest), relevant redacted RPC logs, and txid — never share private keys or wallet passphrases.
