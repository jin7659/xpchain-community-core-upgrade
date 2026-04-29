// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_UTIL_BIP32_H
#define XPCHAIN_UTIL_BIP32_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * Parse an HD keypaths like "m/7/0'/2000".
 */
bool ParseHDKeypath(const std::string& keypath_str, std::vector<uint32_t>& keypath);

/**
 * Format an HD keypath (list of indices) as a human-readable string like "m/7/0'/2000".
 */
std::string FormatHDKeypath(const std::vector<uint32_t>& path);

#endif // XPCHAIN_UTIL_BIP32_H
