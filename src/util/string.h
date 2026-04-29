// Copyright (c) 2019-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_UTIL_STRING_H
#define XPCHAIN_UTIL_STRING_H

#include <string>
#include <vector>

/**
 * Join a list of items with a separator.
 */
template <typename T>
std::string Join(const std::vector<T>& list, const std::string& separator,
                 std::function<std::string(const T&)> item_to_string)
{
    std::string result;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) result += separator;
        result += item_to_string(list[i]);
    }
    return result;
}

inline std::string Join(const std::vector<std::string>& list, const std::string& separator)
{
    std::string result;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) result += separator;
        result += list[i];
    }
    return result;
}

#endif // XPCHAIN_UTIL_STRING_H
