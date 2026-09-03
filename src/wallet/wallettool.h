// Copyright (c) 2016-2026 The Bitcoin Core developers
// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_WALLET_WALLETTOOL_H
#define XPCHAIN_WALLET_WALLETTOOL_H

#include <string>

namespace WalletTool {

bool ExecuteWalletToolFunc(const std::string& command, const std::string& name);

} // namespace WalletTool

#endif // XPCHAIN_WALLET_WALLETTOOL_H
