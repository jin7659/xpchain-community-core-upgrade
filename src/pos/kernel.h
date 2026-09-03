// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_POS_KERNEL_H
#define XPCHAIN_POS_KERNEL_H

#include <amount.h>
#include <consensus/params.h>
#include <primitives/transaction.h>
#include <uint256.h>

class CBlock;
class CTxOut;
class COutPoint;

namespace pos {

/** Calculate/verify stake kernel hash against target */
bool CheckStakeKernelHash(unsigned int nBits, uint32_t nTimeBlockFrom, unsigned int nTxPrevOffset, CAmount nAmount, uint64_t n, uint32_t nTimeTx, uint256& hashProofOfStake, const Consensus::Params& params);
bool CheckStakeKernelHash(unsigned int nBits, uint32_t nTimeBlockFrom, unsigned int nTxPrevOffset, CAmount nAmount, uint64_t n, uint32_t nTimeTx, uint256& hashProofOfStake);
bool CheckStakeKernelHash(unsigned int nBits, const CBlock& blockFrom, unsigned int nTxPrevOffset, const CTxOut& txOutPrev, const COutPoint& prevout, uint32_t nTimeTx, uint256& hashProofOfStake);

/** Pure verification function without disk I/O */
bool CheckProofOfStakePure(const CTransaction& txCoinStake,
                           const CTransaction& txPrev,
                           unsigned int nBits,
                           uint32_t nTimeTx,
                           uint32_t nTimeBlockFrom,
                           unsigned int nTxPrevOffset,
                           uint256& hashProofOfStake,
                           const Consensus::Params& params);

/** Contextual check proof of stake (retrieves txPrev & blockFrom from disk) */
bool CheckProofOfStake(const CTransactionRef& tx, unsigned int nBits, uint256& hashProofOfStake, unsigned int nBlockTime, const Consensus::Params& params);
bool CheckProofOfStake(const CTransactionRef& tx, unsigned int nBits, uint256& hashProofOfStake, unsigned int nBlockTime);

} // namespace pos

using pos::CheckStakeKernelHash;
using pos::CheckProofOfStakePure;
using pos::CheckProofOfStake;

#endif // XPCHAIN_POS_KERNEL_H
