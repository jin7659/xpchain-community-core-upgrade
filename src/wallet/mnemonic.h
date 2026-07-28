// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_WALLET_MNEMONIC_H
#define XPCHAIN_WALLET_MNEMONIC_H

#include <support/allocators/secure.h>

#include <string>
#include <vector>

namespace Mnemonic {
bool Validate(const SecureString& mnemonic, std::string& error_msg);
bool Validate(const std::string& mnemonic, std::string& error_msg);

std::vector<unsigned char> DeriveSeed(const SecureString& mnemonic, const SecureString& passphrase = SecureString());
std::vector<unsigned char> DeriveSeed(const std::string& mnemonic, const std::string& passphrase = "");
} // namespace Mnemonic

#endif // XPCHAIN_WALLET_MNEMONIC_H
