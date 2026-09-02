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

| Still elsewhere | Where | Roadmap stage |
|---|---|---|
| `IsPoSHeight`, `GetAnnualRate`, `GetProofOfStakeReward` | `validation.cpp` | P1-3 |
| `VerifyCoinBaseTx`, `GetRewardHash`, `CheckBlockSignature`, `GetPubKeysFromCoinStakeTx`, `GetPubKeyFromScript`, `MakeBlockHashExcludedSignature` | `validation.cpp` | P1-3 |
| The PoS branches of `ConnectBlock` and `CheckBlock`, and the `fProofOfStake` parameter on `ReadBlockFromDisk` | `validation.cpp` | P1-4, P5-1 |
| The height-gated `CheckProofOfWork` call when loading the block index | `txdb.cpp` | P1-4 |
| The PoS difficulty branches of `GetNextWorkRequired` and `CalculateNextWorkRequired`, plus `GetnBits` which reimplements the same rule | `pow.cpp`, `miner.cpp` | P5-2 |
| The staking thread, `SignBlock`, `CreateTxSig`, `GetRewardPct` and the PoS `CreateNewBlock` overloads | `miner.cpp` | P2, P5-3 |
| `CWallet::CreateCoinStake`, `m_coinstaketx`, `vRewardDistributionPcts` | `wallet/wallet.cpp` | P2 |
| Stake probability and coin-day estimation | `kernelrecord.{h,cpp}` | P2-3 |
| `listmintings` | `wallet/rpcwallet.cpp` | P2-5 |
| Minting tab and staking reward distribution UI | `src/qt/minting*`, `src/qt/stakingrewardsetting*` | P2-3 |

`nSwitchHeight`, `nStakeMinAge` and `nStakeMaxAge` stay in `consensus/params.h` alongside the
other consensus parameters; they are not moved here.

A side benefit of the `pos/` prefix: upstream Bitcoin Core now has its own `src/kernel/`
directory for `libbitcoinkernel`, which a top-level `kernel.h` would have collided with.

## Rules for code in here

1. No behaviour changes without community agreement. Every rule in here is consensus.
2. Prefer explicit parameters over reaching for `chainActive`, `mapBlockIndex` or
   `Params()`. The long-term goal is that PoS verification is a function of its
   arguments, so that it survives the upstream refactors that removed those globals.
3. Node code must not gain a dependency on the wallet. Staking needs a wallet, so that
   direction is inverted through an interface instead (roadmap P2).
