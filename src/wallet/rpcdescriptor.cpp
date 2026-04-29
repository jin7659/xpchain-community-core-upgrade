// Copyright (c) 2018-2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/util.h>
#include <script/descriptor.h>
#include <univalue.h>
#include <utilstrencodings.h>
#include <key.h>

UniValue getdescriptorinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getdescriptorinfo \"descriptor\"\n"
            "\nAnalyses a descriptor.\n"
            "\nArguments:\n"
            "1. \"descriptor\"             (string, required) The descriptor.\n"
            "\nResult:\n"
            "{\n"
            "  \"descriptor\": \"desc\",         (string) The descriptor in canonical form, without private keys\n"
            "  \"checksum\": \"chksum\",        (string) The checksum for the un-checksummed descriptor\n"
            "  \"isrange\": true|false,       (boolean) Whether the descriptor is ranged\n"
            "  \"issolvable\": true|false,    (boolean) Whether the descriptor is solvable\n"
            "  \"hasprivatekeys\": true|false (boolean) Whether the input descriptor contained at least one private key\n"
            "}\n"
        );

    std::string desc_str = request.params[0].get_str();
    FlatSigningProvider provider;
    std::string error;
    auto desc = Parse(desc_str, provider, error);
    if (!desc) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, error);
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("descriptor", desc->ToString());
    result.pushKV("checksum", GetDescriptorChecksum(desc_str));
    result.pushKV("isrange", desc->IsRange());
    result.pushKV("issolvable", desc->IsSolvable());
    result.pushKV("hasprivatekeys", provider.keys.size() > 0);
    return result;
}
