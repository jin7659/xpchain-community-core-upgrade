// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_POS_STAKER_H
#define XPCHAIN_POS_STAKER_H

#include <pos/stakeable_wallet.h>

#include <boost/thread.hpp>
#include <memory>

namespace pos {

/** Start the staking thread for a given stakeable wallet */
void MintStake(boost::thread_group& threadGroup, const std::shared_ptr<IStakeableWallet>& wallet);

} // namespace pos

#endif // XPCHAIN_POS_STAKER_H
