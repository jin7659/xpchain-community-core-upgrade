// Copyright (c) 2026 The XPChain Community developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_POS_HEIGHT_H
#define XPCHAIN_POS_HEIGHT_H

#include <consensus/params.h>

namespace pos {

/**
 * Returns true if the block height is above the PoS switch height.
 */
bool IsPoSHeight(int n, const Consensus::Params& params);

} // namespace pos

using pos::IsPoSHeight;

#endif // XPCHAIN_POS_HEIGHT_H
