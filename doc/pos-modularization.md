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
├── height.h / height.cpp       # Activation height checks and PoS height predicates
├── reward.h / reward.cpp       # Annual interest rates, logistic reward curves, and reward commitments
├── kernel.h / kernel.cpp       # Stake kernel hash validation & pure stateless verification
├── stake.h  / stake.cpp        # Coinstake transaction rules and block signature extraction
└── staker.h / staker.cpp       # Independent PoS block minter thread & candidate dispatcher
```

### Module Responsibilities

| Module | Core Functions | Description |
|--------|---------------|-------------|
| `pos/height` | `IsPoSHeight`, `IsPoWHeight` | Determines whether a given block height belongs to the PoW or PoS consensus era based on consensus parameters. |
| `pos/reward` | `GetAnnualRate`, `GetProofOfStakeReward`, `GetRewardHash` | Implements the logistic reward curve, calculates maturity-dependent staking yields, and builds the reward commitment hash. |
| `pos/kernel` | `CheckStakeKernelHash`, `CheckProofOfStakePure` | Validates that a candidate UTXO meets the PoS difficulty target. `CheckProofOfStakePure` provides an in-memory, pure functional verification path. |
| `pos/stake` | `CheckBlockSignature`, `GetPubKeysFromCoinStakeTx` | Enforces BIP-style block signature rules and verifies coinstake input-to-output pairing. |
| `pos/staker` | `MintStake`, `ThreadStakeMinter` | Background minter thread that searches stake candidates and triggers `BlockAssembler::CreateNewBlock`. |

---

## 3. Decoupling Miner from CWallet via `pos::IStakeableWallet`

### Interface Definition (`src/pos/staker.h`)

To eliminate direct dependencies between block assembly and concrete wallet implementations, the `pos::IStakeableWallet` abstract interface was introduced:

```cpp
namespace pos {

struct StakeCandidate {
    COutPoint outpoint;
    CTxOut txout;
    uint256 hashBlock;
    int64_t nTime;
};

class IStakeableWallet {
public:
    virtual ~IStakeableWallet() = default;
    virtual bool IsLocked() const = 0;
    virtual const char* GetWalletName() const = 0;
    virtual void GetStakeCandidates(std::vector<StakeCandidate>& vCandidates) = 0;
    virtual bool CreateCoinStake(const StakeCandidate& candidate, CTransactionRef& txCoinStake, CAmount& nFees) = 0;
    virtual bool GetPrevTx(const uint256& txHash, CTransactionRef& prevTx, uint256& hashBlock) = 0;
    virtual std::vector<std::pair<CTxDestination, int>> GetRewardPct(const CTxDestination& dest) = 0;
    virtual bool SignReward(uint32_t nTime, const CTransactionRef& coinstake, const std::vector<std::pair<CScript, CAmount>>& rewards, CScript& txSig) = 0;
};

} // namespace pos
```

### Implementation Details
* `CWallet` in `src/wallet/wallet.h` inherits and implements `pos::IStakeableWallet`.
* `BlockAssembler::CreateNewBlock` receives a `pos::IStakeableWallet*` pointer instead of a raw `CWallet*`.
* When compiled with `--disable-wallet`, `BlockAssembler` accepts `nullptr`, allowing non-wallet nodes and testing utilities to compile and link cleanly.

---

## 4. Testing & Verification

The modularized architecture is covered by comprehensive automated tests:
1. **Unit Tests (`src/test/pos_tests.cpp`)**:
   - 326 unit test cases covering kernel hash verification, reward curve saturation, destination matching, and reward preimage layout.
2. **End-to-End Functional Test (`test/functional/feature_pos_staking.py`)**:
   - Validates the entire life cycle: PoW range mining (heights 1..1680) → PoW rejection at switch height → PoS block creation → P2P block propagation → `-reindex` disk revalidation.
