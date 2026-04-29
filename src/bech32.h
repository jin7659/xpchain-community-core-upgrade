// Copyright (c) 2017 Pieter Wuille
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Bech32 is a string encoding format used in newer address types.
// The output consists of a human-readable part (alphanumeric), a
// separator character (1), and a base32 data section, the last
// 6 characters of which are a checksum.
//
// For more information, see BIP 173.

#ifndef BITCOIN_BECH32_H
#define BITCOIN_BECH32_H

#include <stdint.h>
#include <string>
#include <vector>

enum class Encoding {
    INVALID,
    BECH32,
    BECH32M,
};

namespace bech32
{

/** Encode a Bech32 or Bech32m string. Returns the empty string in case of failure. */
std::string Encode(Encoding encoding, const std::string& hrp, const std::vector<uint8_t>& values);

/** Decode a Bech32 or Bech32m string. Returns (encoding, hrp, data). */
struct DecodeResult
{
    Encoding encoding;
    std::string hrp;
    std::vector<uint8_t> data;

    DecodeResult() : encoding(Encoding::INVALID) {}
    DecodeResult(Encoding enc, std::string h, std::vector<uint8_t> d) : encoding(enc), hrp(std::move(h)), data(std::move(d)) {}
};

DecodeResult Decode(const std::string& str);

} // namespace bech32

#endif // BITCOIN_BECH32_H
