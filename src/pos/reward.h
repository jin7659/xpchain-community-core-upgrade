// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_POS_REWARD_H
#define XPCHAIN_POS_REWARD_H

#include <amount.h>
#include <consensus/params.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

#include <cmath>
#include <vector>
#include <utility>

namespace pos {

/** Annual staking rate for a PoS height, as a fraction (0.10 .. 0.05). Zero below nSwitchHeight. */
double_t GetAnnualRate(int nHeight, const Consensus::Params& consensusParams);

/** Calculate PoS staking block reward for the given height, staked amount, and coin age. */
CAmount GetProofOfStakeReward(int nHeight, CAmount nAmount, uint32_t nTime, const Consensus::Params& consensusParams);

/** Calculate reward hash for the reward distribution. */
uint256 GetRewardHash(const std::vector<std::pair<CScript, CAmount>>& vReward, CTransactionRef txCoinStake, uint32_t nTime);

} // namespace pos

using pos::GetAnnualRate;
using pos::GetProofOfStakeReward;
using pos::GetRewardHash;

#endif // XPCHAIN_POS_REWARD_H
