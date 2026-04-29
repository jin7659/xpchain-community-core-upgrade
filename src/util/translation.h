// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_TRANSLATION_H
#define BITCOIN_UTIL_TRANSLATION_H

#include <util/error.h>

#include <functional>
#include <string>

/**
 * Translation function.
 * If no translation function is set, the original English string is returned.
 */
inline bilingual_str _(const char* str)
{
    return bilingual_str{str, str};
}

inline bilingual_str _(const std::string& str)
{
    return bilingual_str{str, str};
}

#endif // BITCOIN_UTIL_TRANSLATION_H
