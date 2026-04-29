// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_UTIL_MESSAGE_H
#define XPCHAIN_UTIL_MESSAGE_H

#include <string>

class CKey;

// The result of a signed message verification.
enum class MessageVerificationResult {
    //! The message was not signed correctly (e.g. the signature is corrupt).
    ERR_MALFORMED_SIGNATURE,
    //! The provided address is invalid.
    ERR_INVALID_ADDRESS,
    //! The provided address is valid but does not refer to a public key.
    ERR_ADDRESS_NO_KEY,
    //! The message verification failed (bad signature).
    ERR_NOT_SIGNED,
    //! The message verification was successful.
    OK,
};

/** Sign a message. Returns true if signing was successful. */
bool MessageSign(const CKey& privkey, const std::string& message, std::string& signature);

/** Verify a signed message. */
MessageVerificationResult MessageVerify(
    const std::string& address,
    const std::string& signature,
    const std::string& message);

/** Prefix that is prepended to a message before it is signed. */
extern const std::string MESSAGE_MAGIC;

#endif // XPCHAIN_UTIL_MESSAGE_H
