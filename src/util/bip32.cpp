// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/bip32.h>

#include <sstream>
#include <stdint.h>
#include <string>
#include <vector>

bool ParseHDKeypath(const std::string& keypath_str, std::vector<uint32_t>& keypath)
{
    std::stringstream ss(keypath_str);
    std::string item;
    keypath.clear();

    // Skip leading "m/" if present
    if (keypath_str.substr(0, 2) == "m/") {
        std::getline(ss, item, '/'); // consume "m"
    }

    while (std::getline(ss, item, '/')) {
        if (item.empty()) return false;
        bool hardened = false;
        if (item.back() == '\'' || item.back() == 'h') {
            hardened = true;
            item.pop_back();
        }
        try {
            uint32_t idx = static_cast<uint32_t>(std::stoul(item));
            if (hardened) idx |= 0x80000000;
            keypath.push_back(idx);
        } catch (...) {
            return false;
        }
    }
    return true;
}

std::string FormatHDKeypath(const std::vector<uint32_t>& path)
{
    std::string ret;
    for (auto& step : path) {
        if (!ret.empty()) ret += "/";
        if (step >> 31) {
            ret += std::to_string(step ^ 0x80000000);
            ret += "'";
        } else {
            ret += std::to_string(step);
        }
    }
    return ret;
}
