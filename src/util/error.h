// Copyright (c) 2010-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_ERROR_H
#define BITCOIN_UTIL_ERROR_H

#include <string>

/**
 * Bilingual messages:
 *   - original:   message in the native language of the developer (English)
 *   - translated: message in the native language of the user (localized)
 * The translated string is intended for end-users, and the original is
 * intended for developers and log files.
 */
struct bilingual_str {
    std::string original;
    std::string translated;

    bilingual_str() {}
    explicit bilingual_str(const std::string& str) : original(str), translated(str) {}
    bilingual_str(const std::string& orig, const std::string& trans)
        : original(orig), translated(trans) {}

    bilingual_str& operator+=(const bilingual_str& rhs)
    {
        original += rhs.original;
        translated += rhs.translated;
        return *this;
    }

    bool empty() const { return original.empty(); }
};

inline bilingual_str operator+(bilingual_str lhs, const bilingual_str& rhs)
{
    lhs += rhs;
    return lhs;
}

/** Mark a bilingual_str as untranslated (original == translated). */
inline bilingual_str Untranslated(const std::string& str)
{
    return bilingual_str{str, str};
}

#endif // BITCOIN_UTIL_ERROR_H
