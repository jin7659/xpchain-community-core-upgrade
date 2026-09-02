# XPChain PoS 합의 규칙 명세 (P0-3)

이 문서는 [분리 로드맵](architecture-separation-roadmap.md)의 **P0-3(합의 파라미터 문서)** 산출물이다.
목적은 "PoS를 지키면서 비-PoS 코드를 이식"할 때, **무엇이 바뀌면 안 되는지**를 코드와 독립적으로
못 박아두는 것이다. P1 이후의 모든 리팩터는 이 문서에 적힌 값·수식·불변식을 바꾸지 않아야 한다.

기준 리비전: `src/validation.cpp`, `src/kernel.cpp`, `src/policy/stake.cpp`, `src/chainparams.cpp`
(XPChain Core 0.27.0, Bitcoin Core 0.17.0 기반).

> **주의:** 아래 값과 수식은 현재 메인넷을 검증하는 동작 그 자체다. 개선이 필요해 보이는 지점에는
> "관찰"로 표시해 두었으며, 이는 **커뮤니티 합의 없이 고칠 수 없다**(로드맵 §2-4).

---

## 1. PoW → PoS 전환

PoS는 별도 체인·별도 헤더가 아니라 **높이 기반 전환**이다.

```
IsPoSHeight(n) := n > nSwitchHeight        // src/validation.cpp
```

| 네트워크 | `nSwitchHeight` | 첫 PoS 높이 |
|---|---|---|
| main | 10275 | 10276 |
| test | 10275 | 10276 |
| regtest | 1680 | 1681 |

즉 `nSwitchHeight` 이하는 PoW 전용, 초과는 **PoS 전용**이다. 혼합 구간은 없다.

### 1.1 블록 헤더는 Bitcoin과 완전히 동일

`CBlockHeader`는 `nVersion / hashPrevBlock / hashMerkleRoot / nTime / nBits / nNonce` 80바이트
그대로이며 PoS 필드가 없다(`src/primitives/block.h`). `CTransaction`에도 `nTime`이 없다.

결과적으로 다음이 **모두 바닐라 Bitcoin과 호환**된다. 이식 시 건드릴 필요가 없다.

- 헤더 직렬화, `CDiskBlockIndex`, headers-first 동기화, compact block
- `nChainWork` 누적과 `FindMostWorkChain` 기반 체인 선택 (PoS용 `nChainTrust`/stake modifier가 없다)

PoS 고유 데이터는 **블록 헤더가 아니라 코인베이스 출력과 코인스테이크 트랜잭션**에 들어간다.

### 1.2 작업증명 검사의 우회

PoS 높이에서는 `CheckProofOfWork()`를 호출하지 않는다. 현재 이 분기는 아래 다섯 지점에 흩어져 있다.

| 위치 | 방식 |
|---|---|
| `CheckBlock()` | `CheckBlockHeader(..., fCheckPOW && !IsPoSHeight(nHeight))` |
| `AcceptBlockHeader()` 경로 | `CheckBlockHeader(..., !IsPoSHeight(pindexPrev->nHeight + 1))` |
| `ReadBlockFromDisk(pos, ...)` | `bool fProofOfStake` 인자로 PoW 검사 생략 |
| `ReadBlockFromDisk(pindex, ...)` | `IsPoSHeight(pindex->nHeight)`를 계산해 위로 전달 |
| `CBlockTreeDB::LoadBlockIndexGuts()` (`src/txdb.cpp:277`) | `nHeight <= nSwitchHeight`일 때만 검사 |

마지막 지점은 `IsPoSHeight()`를 쓰지 않고 `<= nSwitchHeight` 비교를 직접 손으로 적어놨다.
의미는 동일하지만(§1의 `>` 비교의 여집합) **비교 방향이 헷갈리기 쉬운 중복 표현**이므로,
P1에서 나머지 네 지점과 함께 단일 헬퍼로 모아야 한다.

**관찰 (P1-4에서 정리 대상):** `CheckBlock()`은 Bitcoin에서 문맥 독립(context-free) 함수인데,
XPChain은 그 안에서 `mapBlockIndex.find(block.hashPrevBlock)`로 높이를 역추적한다.
부모 헤더를 모르면 `nHeight = 0`으로 떨어져 PoS 블록에 PoW 검사가 적용된다.
headers-first 동기화에서는 부모 헤더가 항상 먼저 오므로 실동작에는 문제가 없지만,
`cs_main` 없이 전역 맵을 읽는 형태이며 최신 Bitcoin(`BlockManager` 분리)으로 이식할 때 그대로 옮길 수 없다.
P1에서 높이(또는 `pindexPrev`)를 **명시적 인자로 전달**하는 형태로 바꾼다. 규칙 자체는 불변이다.

---

## 2. PoS 블록의 구조 규칙

PoS 높이의 블록은 다음을 만족해야 한다.

1. `block.vtx.size() >= 2` — 코인베이스와 코인스테이크 (`CheckBlock`)
2. `block.vtx[1]`이 유효한 코인스테이크 (`IsCoinStakeTx`, `src/policy/stake.cpp`)
   - `vin.size() == 1`
   - `vout.size() == 1`
   - `IsDestinationSame(prevTx->vout[n].scriptPubKey, vout[0].scriptPubKey)`
     — 스테이크한 UTXO와 **동일한 목적지로 되돌려 보내야** 한다
3. `BLOCK_SIGNATURE_ADDITION` 활성 시 (`ConnectBlock`)
   - `CheckBlockSignature(block)` 성공
   - `block.nNonce == 0`
4. 코인베이스 출력이 3개 이상이면 `VerifyCoinBaseTx(block)` 성공
5. 코인베이스 출력이 1~2개이면
   - `vout[0].nValue >= blockReward`
   - `IsDestinationSame(vtx[1]->vout[0].scriptPubKey, vtx[0]->vout[0].scriptPubKey)`
6. 항상: `vtx[0]->GetValueOut() <= blockReward`

`IsDestinationSame(a, b)`는 `Solver()` 결과가 각각 정확히 1개의 solution을 가지고
`ExtractDestination()`이 같은 `CTxDestination`을 낼 때만 참이다.

### 2.1 다중 수령자 코인베이스 (`VerifyCoinBaseTx`)

코인베이스 `vout[0].scriptPubKey`는 `OP_RETURN <출력개수> <서명> <pubkey>` 형태의 데이터 출력이다.

- `vout[0].nValue == 0`, `vout.back().nValue == 0` (witness commitment)
- `<출력개수> == vout.size() - 2`
- `EqualDestination(vtx[1], pubkey)` — pubkey가 코인스테이크 출력의 목적지와 일치
  (P2SH-P2WPKH / P2PKH / P2WPKH 지원)
- `pubkey.Verify(GetRewardHash(rewardValues, vtx[1], block.nTime), <서명>)`

여기서 `rewardValues`는 `vout[1..size]`의 `(scriptPubKey, nValue)` 목록이고,

```
GetRewardHash := Hash( Σ(scriptPubKey ‖ nValue) ‖ nTime ‖ vtx[1]->vin[0] )   // SER_GETHASH
```

이 형태는 예외가 아니라 **기본 경로**다. 마인터의 `GetRewardPct()`(`src/miner.cpp`)가
지갑의 `vRewardDistributionPcts`(스테이킹 보상 분배 설정)로 수령자 목록을 만들고,
남은 지분을 기본 목적지에 배정하므로 목록은 항상 최소 1개다. 따라서 분배를 설정하지 않은
지갑도 코인베이스 출력이 3개(`OP_RETURN` + 보상 1개 + witness commitment)가 되고
`VerifyCoinBaseTx()`를 통과해야 한다. §2의 5번(출력 1~2개 경로)은 다른 구현이 만든
블록을 위한 경로다.

### 2.2 블록 서명 (`CheckBlockSignature`)

블록 서명은 헤더가 아니라 **코인베이스 `scriptSig` 말미**에 실린다.
`MakeBlockHashExcludedSignature()`가 서명을 잘라낸 코인베이스로 머클루트를 재계산하여
"서명 제외 블록 해시"를 만들고, 코인스테이크 입력에서 뽑은 pubkey들 중 하나로 검증한다.

이 방식 덕분에 헤더 포맷이 바닐라로 유지된다. 이식 시 이 성질을 깨지 않아야 한다.

### 2.3 Taproot 출력은 스테이킹할 수 없다 (제품 영향 있음)

`GetPubKeysFromCoinStakeTx()` → `GetPubKeyFromScript()`(`src/validation.cpp`)가 처리하는
스크립트 타입은 다음뿐이다.

`TX_PUBKEY`, `TX_MULTISIG`, `TX_PUBKEYHASH`, `TX_WITNESS_V0_KEYHASH`,
`TX_SCRIPTHASH`(재귀), `TX_WITNESS_V0_SCRIPTHASH`(재귀)

`TX_WITNESS_V1_TAPROOT`는 `default: return false`로 떨어진다. 따라서
**코인스테이크 출력이 bech32m(Taproot)이면 `CheckBlockSignature()`가 절대 성공할 수 없고,
그 블록은 유효할 수 없다.** §2.1의 `EqualDestination()`도 P2SH-P2WPKH / P2PKH / P2WPKH만 안다.

그런데 지갑의 기본 주소 타입은 Taproot다.

```
src/wallet/wallet.h:87
constexpr OutputType DEFAULT_ADDRESS_TYPE{OutputType::BECH32M};
```

이 기본값에는 `TaprootHeight` 활성 여부에 대한 게이트가 없다(`src/wallet/` 전체에
`TaprootHeight` 참조가 없다). 즉 **`getnewaddress`를 기본값으로 받은 주소의 코인은 스테이킹할 수
없다.** 스테이킹하려면 `-addresstype=bech32`(또는 legacy / p2sh-segwit)를 쓰거나
`getnewaddress "" "bech32"`로 주소를 받아야 한다.

증상이 늦게 드러나는 것도 문제다. 코인스테이크 **구조** 검사(`IsCoinStakeTx`,
`IsDestinationSame`)는 Taproot 목적지를 정상 수락하므로, 마인터는 커널을 찾고 코인스테이크를
만들고 블록을 조립한 뒤 서명 단계에서 조용히 실패한다. 로그에는
`SignStep: CreateSig failed for Taproot` / `pubkey hash not found`만 남는다.

이 제약은 다음 두 로드맵 항목의 선행 조건이다.

- **P3-5 (Taproot 지갑 경로)** — 기본 주소 타입을 이대로 두면 신규 사용자의 잔고가
 스테이킹 불가 상태가 된다. 기본값 변경 또는 PoS 서명 경로의 Taproot 지원 중 하나를 골라야 한다.
- **P5-5 (`TaprootHeight` 메인넷 활성 정책)** — PoS 서명 규칙에 Taproot를 추가하는 것은
 **합의 변경**이다. 활성 높이 결정과 함께 다뤄야 한다.

현재 동작은 `src/test/pos_tests.cpp`의 `coinstake_pubkey_extraction_rejects_taproot`가
고정하고 있다. 지원을 추가하려면 이 테스트를 의도적으로 갱신해야 한다.

---

## 3. 커널 해시 (`CheckStakeKernelHash`)

`src/kernel.cpp`. PoS의 심장이며 **바이트 단위로 보존해야 하는 유일한 해시 규칙**이다.

### 3.1 나이 요건

```
nTimeBlockFrom + nStakeMinAge > nTimeTx  ⇒  실패
```

| 네트워크 | `nStakeMinAge` | `nStakeMaxAge` |
|---|---|---|
| main | 259 200 (3일) | 5 184 000 (60일) |
| test | 259 200 (3일) | 5 184 000 (60일) |
| regtest | 10 (10초) | 8 640 000 (100일) |

### 3.2 해시 입력 (직렬화 순서)

`CDataStream(SER_GETHASH, 0)`에 아래 순서로 쓰고 `Hash()`(double-SHA256)를 취한다. 총 28바이트.

| # | 값 | 타입 | 바이트 |
|---|---|---|---|
| 1 | `nBits` | `unsigned int` | 4 (LE) |
| 2 | `nTimeBlockFrom` | `uint32_t` | 4 (LE) |
| 3 | `nTxPrevOffset` | `unsigned int` | 4 (LE) |
| 4 | `nTimeBlockFrom` | `uint32_t` | 4 (LE) |
| 5 | `n` (prevout index) | `uint64_t` | 8 (LE) |
| 6 | `nTimeTx` | `uint32_t` | 4 (LE) |

**`nTimeBlockFrom`이 2번과 4번에 두 번 들어간다.** Peercoin 계열 원본은 4번 자리가
"이전 트랜잭션의 `nTime`"이지만 XPChain은 트랜잭션에 `nTime`이 없으므로 블록 시각으로 대체했다.
중복은 의도된 것이며 **버그로 오인해 고치면 즉시 체인이 갈라진다.**

`nTxPrevOffset`은 이전 블록 안에서 트랜잭션 배열이 시작하는 오프셋이다.

```
nTxPrevOffset = GetSizeOfCompactSize(blockFrom.vtx.size()) + sizeof(CBlockHeader)   // = ... + 80
```

즉 이전 블록의 **트랜잭션 개수**에만 의존한다(개별 tx 위치가 아니다).

### 3.3 성공 조건

```
bnTargetPerCoinDay = arith_uint256().SetCompact(nBits)
nTimeWeight        = min(nTimeTx - nTimeBlockFrom, nStakeMaxAge) - nStakeMinAge
bnCoinDayWeight    = arith_uint256(nAmount) * nTimeWeight / COIN / 86400      // 정수 나눗셈

성공 ⟺ uint512(hashProofOfStake) <= uint512(bnCoinDayWeight) * uint512(bnTargetPerCoinDay)
```

- `nAmount`는 스테이크한 UTXO의 `nValue`(satoshi)다.
- 나눗셈이 정수이므로 `nAmount/COIN * nTimeWeight < 86400`이면 `bnCoinDayWeight == 0`이 되어
  **어떤 해시로도 성공할 수 없다.** (regtest 시나리오 설계 시 반드시 고려)
- 512비트 비교는 `arith_uint512`(XPChain 추가, `src/arith_uint256.h`)를 쓴다.

---

## 4. 코인스테이크 전체 검증 (`CheckProofOfStake`)

`ConnectBlock()` 진입 직후 호출된다(`src/validation.cpp`).

```
CheckProofOfStake(block.vtx[1], block.nBits, hashProofOfStake, block.nTime)
```

수행 순서:

1. `GetTransaction(vin[0].prevout.hash, txPrev, ..., /*fAllowSlow=*/true)` — 이전 tx와 그 블록 해시
2. 모든 입력의 이전 출력을 모아 `PrecomputedTransactionData`를 구성
3. `CScriptCheck(txPrev->vout[n], tx, 0, nFlags, true, &txdata)` — 입력 0의 스크립트 검증
   - `nFlags = SCRIPT_VERIFY_NONE`, 단 Taproot 활성 높이 이상이면 `SCRIPT_VERIFY_TAPROOT` 추가
4. `mapBlockIndex`에서 이전 블록 인덱스를 찾고 `ReadBlockFromDisk()`로 **블록 전체를 읽음**
5. `CheckStakeKernelHash(nBits, prevBlock.GetBlockTime(), offset, txPrev->vout[n].nValue, n, block.nTime, ...)`

### 4.1 데이터 의존성 (이식 최대 난관)

4단계는 임의의 과거 블록을 디스크에서 읽고, 1단계는 `-txindex` 또는 UTXO 기반 느린 경로에 의존한다.
최신 Bitcoin은 **검증 경로가 txindex에 의존하지 않는다**는 것을 전제로 하며,
`mapBlockIndex`/`chainActive` 전역도 `BlockManager`/`ChainstateManager`로 대체되었다.

**관찰:** 커널 해시가 실제로 필요한 값은 다음뿐이다.

| 필요한 값 | 현재 출처 | 대체 가능한 출처 |
|---|---|---|
| `nAmount` | `txPrev->vout[n].nValue` (txindex) | `CCoinsViewCache`의 `Coin::out.nValue` |
| `scriptPubKey` (서명 검증용) | `txPrev->vout[n]` (txindex) | `Coin::out.scriptPubKey` |
| `nTimeBlockFrom` | `ReadBlockFromDisk` 후 헤더 | `pindex->nTime` |
| `nTxPrevOffset` | `ReadBlockFromDisk` 후 `vtx.size()` | `pindex->nTx` (`src/chain.h`) |
| 이전 블록 인덱스 | `mapBlockIndex[hashBlock]` | `Coin::nHeight` → 활성 체인 조회 |

`ConnectBlock()` 시점에는 코인스테이크의 입력이 아직 UTXO 집합에 살아 있으므로,
위 값 전부를 **디스크 읽기 없이, txindex 없이** 얻을 수 있다.
이 치환은 규칙을 바꾸지 않으면서 프루닝 호환성과 O(1) 검증을 동시에 얻는다.
P1(경계 고정) → P5(수렴) 사이에서 다룬다.

### 4.2 Taproot 플래그의 문맥 오류

3단계의 플래그 판정이 `chainActive.Height() + 1 >= TaprootHeight`로 되어 있다.
검증 중인 **블록의 높이**가 아니라 **현재 활성 팁의 높이**를 본다.
재구성(reorg)이나 `TestBlockValidity()` 경로에서는 두 값이 다를 수 있어,
노드마다 다른 스크립트 플래그로 같은 블록을 검증할 여지가 있다.

| 네트워크 | `TaprootHeight` |
|---|---|
| main | 3 000 000 |
| test | 0 |
| regtest | 0 |

메인넷은 아직 활성 전이므로 **지금은 실제 분기 위험이 없다.**
높이 3 000 000 이전에 `pindex->nHeight` 기준으로 고쳐야 한다(로드맵 P5-5와 함께 결정).

같은 함수의 2단계도, 일부 입력의 이전 tx를 못 찾으면 `spent_outputs`를 버리고
`PrecomputedTransactionData(*tx)`로 조용히 되돌아간다. Taproot 활성 후에는 sighash가 달라져
검증이 실패하게 되므로, §4.1의 UTXO 기반 치환으로 이 폴백 자체를 제거하는 것이 옳다.

---

## 5. PoS 보상 (`GetProofOfStakeReward`)

`src/validation.cpp`. `ConnectBlock()`이 계산하는 PoS 블록의 상한 보상이다.

```
blockReward = GetProofOfStakeReward(pindex->nHeight, nAmount, nAge, consensus)

nAmount = txPrev->vout[ vtx[1]->vin[0].prevout.n ].nValue     // 스테이크한 금액
nAge    = block.nTime - prevBlockHeader.nTime                 // uint32_t
```

**PoW 보상과 달리 수수료(`nFees`)를 더하지 않는다.** PoW 구간만
`blockReward = nFees + GetBlockSubsidy(...)`이고, `GetBlockSubsidy`는 `11000000 * COIN`을
`nHeight / nSubsidyHalvingInterval`만큼 우측 시프트한 값이다.

### 5.1 연 이율 (`GetAnnualRate`)

`nSubsidyReducingInterval = 60 * 24 * 365 = 525600` 블록.

| 높이 구간 | 연 이율 |
|---|---|
| PoS 이전 | 0 |
| ~ 525 600 | 0.10 |
| 525 601 ~ 1 051 200 | 0.09 |
| 1 051 201 ~ 1 576 800 | 0.08 |
| 1 576 801 ~ 2 102 400 | 0.07 |
| 2 102 401 ~ 2 628 000 | 0.06 |
| 2 628 001 ~ | 0.05 |

### 5.2 나이 계수와 최종 식

```
M = 1.025   (dRewardCurveMaximum)
B = 0.018   (dRewardCurveBase)
L = 1.0     (dRewardCurveLimit)
S = 0.00000285 (dRewardCurveSteepness)

if nAge < nStakeMinAge:  return 0
nAge = min(nAge, nStakeMaxAge)

coefficient = min( M / (1 + (M/B - 1) * exp(-S * nAge)), L )
reward      = (CAmount)( nAmount * GetAnnualRate(nHeight) * coefficient * nAge / 31536000 )
```

`31536000 = 365 * 24 * 60 * 60`. 마지막 `(CAmount)` 캐스팅은 **0 방향 절단**이다.

성질(P0-2 단위 테스트가 검증):

- `nAge < nStakeMinAge` → 0
- `nAge`에 대해 단조 증가 (`nStakeMaxAge`에서 포화)
- `coefficient`는 `nAge` 하나만의 함수이고, 로지스틱 곡선이 `L = 1.0`에 걸리는
  `nAge ≈ ln((M/B - 1) / (L/M - 1... ))` 지점 이후로는 정확히 1.0으로 고정된다
- PoS 이전 높이 → 0

### 5.3 부동소수점 합의 (최우선 이식 리스크)

`GetAnnualRate`는 `double_t`를 반환하고 보상 계산은 `exp()`와 `double` 산술을 쓴다.
**즉 합의 결과가 컴파일러·libm·최적화 플래그·타깃 아키텍처에 의존한다.**
현재 트리는 이미 C++17(`configure.ac`의 `AX_CXX_COMPILE_STDCXX([17])`)이지만,
최신 Bitcoin Core로 갈수록 요구 툴체인이 올라가므로 이 의존성은 반드시 제거해야 한다.

다행히 위험은 유한하고 검증 가능하다.

- `coefficient`는 **정수 하나(`nAge`)의 함수**다. 정의역은 `[nStakeMinAge, nStakeMaxAge]`,
  메인넷 기준 4 924 801개 값뿐이다 → **전수 차분 테스트로 대체 구현의 동등성을 완전히 증명할 수 있다.**
- `GetAnnualRate`가 반환하는 여섯 값(0.10~0.05)은 유리수로 정확히 표현된다.
- 최종 절단이 정수를 만들므로, 목표는 "같은 정수를 내는 결정적 구현"이다.

로드맵상 이 작업은 **P1 이후 별도 PR**로 분리한다. P0에서는 현재 동작을 골든 벡터로 고정만 한다.

### 5.4 언더플로 불변식 (리팩터 시 깨지기 쉬움)

`nAge`는 `uint32_t`이고 `block.nTime - prevHeader.nTime`으로 계산된다.
`block.nTime < prevHeader.nTime`이면 **언더플로로 거대한 값이 되고, 곧바로 `min(nAge, nStakeMaxAge)`가
이를 `nStakeMaxAge`로 잘라내어 최대 보상을 주게 된다.**

현재 이것이 악용 불가능한 이유는 오직 다음 순서 때문이다.

1. `ConnectBlock()`은 **함수 진입 직후** `CheckProofOfStake()`를 먼저 호출한다.
2. `CheckStakeKernelHash()`가 `nTimeBlockFrom + nStakeMinAge > nTimeTx`를 거부한다.
3. 따라서 이후 보상 계산 지점에서는 `block.nTime >= prevHeader.nTime + nStakeMinAge`가 보장된다.

**이 호출 순서는 합의 불변식이다.** `ConnectBlock`을 재배치하거나 PoS 훅을 분리할 때
(P1-4, P5-1) 순서가 바뀌면 인플레이션 취약점이 열린다. P1의 회귀 테스트가 반드시 덮어야 한다.

---

## 6. 난이도 (`pow.cpp`)

**PoS는 별도의 난이도 조정 알고리즘을 가진다.** 함수 이름과 `nBits` 필드를 PoW와 공유하기
때문에 눈에 잘 안 띄지만, `GetNextWorkRequired()`와 `CalculateNextWorkRequired()` 양쪽에
`pindexLast->nHeight > nSwitchHeight` 분기가 들어 있다.

| 항목 | PoW | PoS |
|---|---|---|
| 조정 주기 | `DifficultyAdjustmentInterval()`(2016)의 배수 높이에서만 | **매 블록** (주기 스킵 분기가 PoW 전용) |
| 관측 창 | 직전 2015블록 (`nHeight - (interval - 1)`) | **직전 1블록** (`nHeight - 1`) |
| `nActualTimespan` 클램프 | `[timespan/4, timespan*4]` | **없음** |
| 최소난이도 예외 (`fPowAllowMinDifficultyBlocks`) | 적용 | 미적용 |
| 재타깃 식 | `bnNew *= actual; bnNew /= nPowTargetTimespan` | 아래 |

```
nInterval = nPowTargetTimespan / nPowTargetSpacing
bnNew *= ((nInterval - 1) * nPowTargetSpacing + nActualTimespan + nActualTimespan);
bnNew /= ((nInterval + 1) * nPowTargetSpacing);
```

즉 매 블록 이전 블록과의 간격만 보고 목표 간격(`nPowTargetSpacing`, 60초)으로 끌어당기는
1차 필터다. `nActualTimespan`이 0에 가까우면 배율이 `(nInterval-1)/(nInterval+1)`로 수렴하므로
블록당 최대 감쇠폭이 유한하다. 상한은 PoW와 동일한 `powLimit`이다.

`fPowNoRetargeting`(regtest)은 `CalculateNextWorkRequired()` 진입 즉시 `pindexLast->nBits`를
반환하므로 PoS 경로에도 그대로 적용된다.

PoS 전용 `posLimit`은 없고, 계산된 `nBits`가 §3.3의 커널 해시 목표로 재사용된다.

**관찰:** 스테이킹 측 `GetnBits()`(`src/miner.cpp`)는 `GetNextWorkRequired()`를 거치지 않고
`CalculateNextWorkRequired(pindexLast, pindexLast->pprev->GetBlockTime(), params)`를 직접
호출한다. PoS 분기의 관측 창이 1블록이므로 결과는 같지만, **두 곳에 같은 규칙이 중복 표현되어
있다.** 로드맵 P5-2(`pos/difficulty.cpp`)에서 하나로 합쳐야 한다.

---

## 7. XPChain 고유 버전비트 배포

`src/consensus/params.h`의 `DeploymentPos`에 Bitcoin에 없는 항목이 있다.

| 배포 | bit | main / test | regtest |
|---|---|---|---|
| `DEPLOYMENT_CHECK_DUP_TXIN` | 3 | — | ALWAYS_ACTIVE |
| `BLOCK_SIGNATURE_ADDITION` | 2 | 2019-04-01 ~ 2020-04-01 | ALWAYS_ACTIVE |
| `DEPLOYMENT_TAPROOT` | — | 높이 기반 (`TaprootHeight`) | 0 |

`BLOCK_SIGNATURE_ADDITION`은 메인넷에서 이미 잠긴 소프트포크이며 §2.2의 규칙을 활성화한다.

---

## 8. 이식 시 절대 바뀌면 안 되는 목록 (체크리스트)

| # | 항목 | 근거 |
|---|---|---|
| 1 | `nSwitchHeight` 및 `IsPoSHeight`의 **초과(`>`)** 비교 | §1 |
| 2 | 커널 해시의 6개 필드 순서·타입, `nTimeBlockFrom` 중복 | §3.2 |
| 3 | `nTxPrevOffset` 계산식 (compact size + 80) | §3.2 |
| 4 | `nTimeWeight` / `bnCoinDayWeight`의 **정수 나눗셈** 순서 | §3.3 |
| 5 | 512비트 비교(256비트로 줄이면 오버플로 거동이 달라짐) | §3.3 |
| 6 | 코인스테이크 구조 규칙 (`vin==1`, `vout==1`, 동일 목적지) | §2 |
| 7 | `GetRewardHash` 직렬화, 블록 서명 제외 해시 계산 | §2.1, §2.2 |
| 8 | 보상 공식의 상수·연산 순서·최종 절단 | §5.2 |
| 9 | PoS 보상에 **수수료를 더하지 않음** | §5 |
| 10 | `CheckProofOfStake` → 보상 계산의 **호출 순서** | §5.4 |
| 11 | PoS 재타깃: 매 블록 / 1블록 창 / 클램프 없음 / 전용 식 | §6 |
| 12 | PoW 검사를 건너뛰는 다섯 지점 (`txdb.cpp` 포함) | §1.2 |

---

## 9. 관련 문서

- [분리 방식 절차 (P0~P6 로드맵)](architecture-separation-roadmap.md)
- 단위 테스트: `src/test/pos_tests.cpp` (P0-2)
- 기능 테스트: `test/functional/feature_pos_staking.py` (P0-4)
