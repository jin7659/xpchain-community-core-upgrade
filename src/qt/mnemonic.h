// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_MNEMONIC_H
#define XPCHAIN_QT_MNEMONIC_H

#include <string>
#include <vector>

namespace Mnemonic {
    // Validates if the given mnemonic sentence is standard BIP39 and passes checksum check.
    // If it fails, error_msg will be populated with the reason.
    bool Validate(const std::string& mnemonic, std::string& error_msg);

    // Generates 64-byte HD seed from the mnemonic and an optional passphrase.
    // Derived seed size is always 64 bytes (512 bits) as per BIP39 specification.
    std::vector<unsigned char> DeriveSeed(const std::string& mnemonic, const std::string& passphrase = "");
}

#endif // XPCHAIN_QT_MNEMONIC_H
