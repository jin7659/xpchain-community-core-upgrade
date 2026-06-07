// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mnemonic.h>
#include <qt/res/bip39_words.h>
#include <crypto/sha256.h>
#include <crypto/hmac_sha512.h>

#include <sstream>
#include <algorithm>
#include <cstring>
#include <vector>
#include <unordered_map>

namespace Mnemonic {

// Helper function to split string by space and convert to lowercase
static std::vector<std::string> SplitWords(const std::string& sentence) {
    std::vector<std::string> words;
    std::stringstream ss(sentence);
    std::string word;
    while (ss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        words.push_back(word);
    }
    return words;
}

// Find word index in BIP39_WORDS
static int GetWordIndex(const std::string& word) {
    static const std::unordered_map<std::string, int> word_map = []() {
        std::unordered_map<std::string, int> map;
        map.reserve(2048);
        for (int i = 0; i < 2048; ++i) {
            map.emplace(BIP39_WORDS[i], i);
        }
        return map;
    }();
    const auto it = word_map.find(word);
    if (it == word_map.end()) {
        return -1;
    }
    return it->second;
}

bool Validate(const std::string& mnemonic, std::string& error_msg) {
    SecureString secure(mnemonic.begin(), mnemonic.end());
    return Validate(secure, error_msg);
}

bool Validate(const SecureString& mnemonic, std::string& error_msg) {
    std::vector<std::string> words = SplitWords(std::string(mnemonic.begin(), mnemonic.end()));
    if (words.size() != 12 && words.size() != 24) {
        error_msg = "Mnemonic must be exactly 12 or 24 words.";
        return false;
    }

    std::vector<int> word_indices;
    for (const auto& w : words) {
        int idx = GetWordIndex(w);
        if (idx == -1) {
            error_msg = "Invalid word found in mnemonic: " + w;
            return false;
        }
        word_indices.push_back(idx);
    }

    int total_bits = words.size() * 11;
    std::vector<bool> bits(total_bits);
    for (size_t i = 0; i < word_indices.size(); ++i) {
        int idx = word_indices[i];
        for (int b = 0; b < 11; ++b) {
            bits[i * 11 + b] = (idx >> (10 - b)) & 1;
        }
    }

    int entropy_bits_len = (words.size() == 12) ? 128 : 256;
    int checksum_bits_len = (words.size() == 12) ? 4 : 8;

    std::vector<unsigned char> entropy(entropy_bits_len / 8, 0);
    for (int i = 0; i < entropy_bits_len; ++i) {
        if (bits[i]) {
            entropy[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    unsigned char hash[32];
    CSHA256 sha;
    sha.Write(entropy.data(), entropy.size());
    sha.Finalize(hash);

    int mnemonic_checksum = 0;
    for (int i = 0; i < checksum_bits_len; ++i) {
        mnemonic_checksum <<= 1;
        if (bits[entropy_bits_len + i]) {
            mnemonic_checksum |= 1;
        }
    }

    int calculated_checksum = 0;
    if (words.size() == 12) {
        calculated_checksum = (hash[0] >> 4) & 0x0F;
    } else {
        calculated_checksum = hash[0];
    }

    if (mnemonic_checksum != calculated_checksum) {
        error_msg = "Mnemonic checksum validation failed. Please check the word sequence and spelling.";
        return false;
    }

    return true;
}

std::vector<unsigned char> DeriveSeed(const std::string& mnemonic, const std::string& passphrase) {
    SecureString mnemonicSec(mnemonic.begin(), mnemonic.end());
    SecureString passphraseSec(passphrase.begin(), passphrase.end());
    return DeriveSeed(mnemonicSec, passphraseSec);
}

std::vector<unsigned char> DeriveSeed(const SecureString& mnemonic, const SecureString& passphrase) {
    // BIP39 Master Seed PBKDF2 parameters:
    // - Password: the mnemonic sentence
    // - Salt: "mnemonic" + passphrase
    // - Iteration count: 2048
    // - Output size: 64 bytes (512 bits)
    
    SecureString salt;
    salt.append("mnemonic");
    salt.append(passphrase);
    
    // We derive 64 bytes
    std::vector<unsigned char> derived_seed(64, 0);
    
    // For PBKDF2-HMAC-SHA512 with dkLen=64 (one block of 64 bytes), we only need 1 block.
    // U_1 = HMAC-SHA512(Password, Salt || INT_32_BE(1))
    // T = U_1 XOR U_2 XOR ... XOR U_2048
    
    std::vector<unsigned char> salt_buf(salt.size() + 4);
    std::memcpy(salt_buf.data(), salt.data(), salt.size());
    salt_buf[salt.size()] = 0;
    salt_buf[salt.size() + 1] = 0;
    salt_buf[salt.size() + 2] = 0;
    salt_buf[salt.size() + 3] = 1; // 1 in 32-bit big endian
    
    unsigned char u[64];
    unsigned char t[64];
    
    CHMAC_SHA512 hmac((const unsigned char*)mnemonic.data(), mnemonic.size());
    hmac.Write(salt_buf.data(), salt_buf.size());
    hmac.Finalize(u);
    
    std::memcpy(t, u, 64);
    
    for (uint32_t i = 1; i < 2048; ++i) {
        CHMAC_SHA512 hmac_step((const unsigned char*)mnemonic.data(), mnemonic.size());
        hmac_step.Write(u, 64);
        hmac_step.Finalize(u);
        for (int j = 0; j < 64; ++j) {
            t[j] ^= u[j];
        }
    }
    
    std::memcpy(derived_seed.data(), t, 64);
    return derived_seed;
}

} // namespace Mnemonic
