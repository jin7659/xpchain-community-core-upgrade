# XPChain Proof-of-Stake Consensus Modularization & Architecture

This document describes the architectural modularization of XPChain Core's Proof-of-Stake (PoS) consensus engine and the decoupling of the miner/assembler from wallet internals via `pos::IStakeableWallet`.

---

## 1. Background and Motivation

In earlier versions of XPChain Core (derived from Bitcoin 0.17.0 and Peercoin/Blackcoin ancestors), PoS consensus verification, reward calculations, block signing, and stake mining were tightly coupled across monolithic files such as `kernel.cpp`, `policy/stake.cpp`, `miner.cpp`, and `validation.cpp`.

This monolithic design had significant drawbacks:
* Direct `#ifdef ENABLE_WALLET` conditional blocks inside `miner.cpp` prevented headless, wallet-disabled builds (`--disable-wallet`).
* Consensus-critical calculations (such as annual reward rates and difficulty retargeting) lacked pure functional boundaries, making unit testing difficult without loading block indexes or disk databases.
* Future major upgrades (e.g. upstream Bitcoin rebases or Taproot staking) required touching heavily entangled files.

The refactoring carried out in the `src/pos/` subsystem cleanly isolates consensus rules, pure verification routines, and staking abstractions.

---

## 2. Module Structure (`src/pos/`)

All PoS consensus logic is organized under the `pos::` namespace across five cohesive submodules:

```
src/pos/
├── height.h / height.cpp             # PoS activation height predicate
├── reward.h / reward.cpp             # Annual interest rates, logistic reward curves, and reward commitments
├── kernel.h / kernel.cpp             # Stake kernel hash validation & in-memory verification
├── stake.h  / stake.cpp              # Coinstake transaction rules and block signature extraction
├── stakeable_wallet.h                # The wallet contract the assembler and the minter depend on
└── staker.h / staker.cpp             # Independent PoS block minter thread & candidate dispatcher
```

### Module Responsibilities

| Module | Core Functions | Description |
|--------|---------------|-------------|
| `pos/height` | `IsPoSHeight` | Determines whether a given block height belongs to the PoW or PoS consensus era based on consensus parameters. |
| `pos/reward` | `GetAnnualRate`, `GetProofOfStakeReward`, `GetRewardHash` | Implements the logistic reward curve, calculates maturity-dependent staking yields, and builds the reward commitment hash. |
| `pos/kernel` | `CheckStakeKernelHash`, `CheckProofOfStakePure`, `CheckProofOfStake` | Validates that a candidate UTXO meets the PoS difficulty target. `CheckProofOfStakePure` takes the previous transaction and block time as arguments instead of reading them from disk; `CheckProofOfStake` is the disk-backed wrapper used by `ConnectBlock`. |
| `pos/stake` | `IsCoinStakeTx`, `IsDestinationSame`, `CheckBlockSignature`, `GetPubKeysFromCoinStakeTx` | Enforces the coinstake transaction shape and the block signature rules, and extracts the signing pubkeys from a coinstake. |
| `pos/stakeable_wallet` | `IStakeableWallet`, `StakeCandidate` | The abstract wallet contract, kept in its own header so that `BlockAssembler` can depend on the contract without depending on the staking thread. |
| `pos/staker` | `MintStake` | Starts the background minter thread that searches stake candidates and triggers `BlockAssembler::CreateNewBlock`. |

All entry points are called with an explicit `pos::` qualifier; the headers deliberately export nothing into the global namespace.

---

## 3. Decoupling Miner from CWallet via `pos::IStakeableWallet`

### Interface Definition (`src/pos/stakeable_wallet.h`)

To eliminate direct dependencies between block assembly and concrete wallet implementations, the `pos::IStakeableWallet` abstract interface was introduced:

```cpp
namespace pos {

struct StakeCandidate {
    COutPoint outpoint;
    CTxOut txout;
    uint256 hashBlock;
};

class IStakeableWallet {
public:
    virtual ~IStakeableWallet() = default;

    virtual std::string GetWalletName() const = 0;
    virtual bool IsLocked() const = 0;
    virtual void GetStakeCandidates(std::vector<StakeCandidate>& vCandidates) = 0;
    virtual bool CreateCoinStake(const StakeCandidate& candidate, CTransactionRef& txNew, CAmount& nFees) = 0;
    virtual bool SignReward(uint32_t nTime, CTransactionRef txCoinStake,
                            const std::vector<std::pair<CScript, CAmount>>& vValues,
                            CScript& script) const = 0;
    virtual bool SignBlock(CBlock* pblock) const = 0;
    virtual std::vector<std::pair<CTxDestination, int>> GetRewardPct(const CTxDestination& defaultDestination) const = 0;
    virtual bool GetPrevTx(const uint256& hash, CTransactionRef& txOut, uint256& hashBlock) const = 0;
};

} // namespace pos
```

### Implementation Details
* `CWallet` in `src/wallet/wallet.h` inherits and implements `pos::IStakeableWallet`.
* `BlockAssembler::CreateNewBlock` receives a `pos::IStakeableWallet*` pointer instead of a raw `CWallet*`.
* When compiled with `--disable-wallet`, `BlockAssembler` accepts `nullptr`, allowing non-wallet nodes and testing utilities to compile and link cleanly.

---

## 4. Testing & Verification

The modularized architecture is covered by automated tests:
1. **Unit Tests (`src/test/pos_tests.cpp`)**: 24 cases covering kernel hash verification, the stake minimum/maximum age boundaries, reward curve saturation, destination matching, and the kernel and reward preimage layouts. They run as part of the 326-case `test_xpchain` suite.
2. **End-to-End Functional Test (`test/functional/feature_pos_staking.py`)**:
   - Validates the entire life cycle: PoW range mining (heights 1..1680) → PoW rejection at switch height → PoS block creation → P2P block propagation → `-reindex` disk revalidation.
3. **Boundary Lint (`test/lint/lint-circular-dependencies.sh`)**: run by the `lint` CI job, so a new include edge that puts `src/pos/` back into a cycle with the node fails the build.

### Boundary debt still outstanding

The module owns the PoS algorithms, but consensus orchestration is still in the node:

* `ConnectBlock` and `CheckBlock` in `validation.cpp` hold the PoS branch, and the split-reward coinbase verifier `VerifyCoinBaseTx` has not moved into `pos/`.
* `pos/kernel` and `pos/stake` still reach back into `validation.h` for chain access (`GetTransaction`, `ReadBlockFromDisk`, `mapBlockIndex`), which is why two `pos -> validation -> pos` cycles remain in the lint's expected list. Removing them means passing chain access in rather than using node globals.
* `CheckProofOfStakePure` reads `chainActive.Height()` to decide whether to set `SCRIPT_VERIFY_TAPROOT`, so its result depends on the current tip rather than on the block being validated.
* `GetAnnualRate` and `GetProofOfStakeReward` compute consensus values in `double_t` using `exp()`.
* `pow.cpp` and `txdb.cpp` compare against `nSwitchHeight` directly instead of calling `pos::IsPoSHeight`.
