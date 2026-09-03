// Copyright (c) 2026 The XPChain Community developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/height.h>

namespace pos {

bool IsPoSHeight(int n, const Consensus::Params& params)
{
    return n > params.nSwitchHeight;
}

} // namespace pos
