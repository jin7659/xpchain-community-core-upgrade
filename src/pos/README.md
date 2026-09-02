# `src/pos/` — Proof-of-stake consensus

This directory holds XPChain's proof-of-stake rules. It exists so that porting non-PoS
code forward from Bitcoin Core touches as little PoS code as possible, and so that
merge conflicts from an upstream import land here rather than being scattered through
`validation.cpp` and `miner.cpp`.

See [`doc/architecture-separation-roadmap.md`](../../doc/architecture-separation-roadmap.md)
for the staged plan and [`doc/xpchain-pos-consensus.md`](../../doc/xpchain-pos-consensus.md)
for the rules themselves, including the list of things that must not change.

## Current contents

| File | Contents |
|---|---|
| `kernel.h` / `kernel.cpp` | The stake kernel hash and full coinstake verification (`CheckStakeKernelHash`, `CheckProofOfStake`) |
| `stake_policy.h` / `stake_policy.cpp` | Coinstake shape rules (`IsCoinStakeTx`, `IsDestinationSame`) |

Regression coverage lives in `src/test/pos_tests.cpp` and
`test/functional/feature_pos_staking.py`.

## Still outside this directory

Moving these is the rest of stage P1 and stage P5 of the roadmap. Listed here so the
boundary is honest about where it currently stops:

- `IsPoSHeight`, `GetAnnualRate`, `GetProofOfStakeReward` — `validation.cpp` (P1-3)
- `VerifyCoinBaseTx`, `GetRewardHash`, `CheckBlockSignature`, `GetPubKeysFromCoinStakeTx` — `validation.cpp` (P1-3)
- The PoS branches of `ConnectBlock` and `CheckBlock` — `validation.cpp` (P1-4, P5-1)
- The staking thread and the PoS `CreateNewBlock` overloads — `miner.cpp` (P2, P5-3)
- `CWallet::CreateCoinStake` — `wallet/wallet.cpp` (P2)

## Rules for code in here

1. No behaviour changes without community agreement. Every rule in here is consensus.
2. Prefer explicit parameters over reaching for `chainActive`, `mapBlockIndex` or
   `Params()`. The long-term goal is that PoS verification is a function of its
   arguments, so that it survives the upstream refactors that removed those globals.
3. Node code must not gain a dependency on the wallet. Staking needs a wallet, so that
   direction is inverted through an interface instead (roadmap P2).
