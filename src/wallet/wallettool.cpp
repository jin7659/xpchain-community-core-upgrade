// Copyright (c) 2016-2026 The Bitcoin Core developers
// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallettool.h>
#include <wallet/wallet.h>
#include <wallet/walletutil.h>
#include <wallet/db.h>
#include <util.h>

#include <iostream>

namespace WalletTool {

static std::shared_ptr<CWallet> MakeWallet(const std::string& name, const fs::path& path, uint64_t flags, bool force_berkeley)
{
    return CWallet::CreateWalletFromFile(name, path, flags, force_berkeley);
}

bool ExecuteWalletToolFunc(const std::string& command, const std::string& name)
{
    fs::path path = fs::absolute(name, GetWalletDir());

    if (command == "create") {
        uint64_t flags = 0;
        if (gArgs.GetBoolArg("-descriptors", false)) {
            flags |= WALLET_FLAG_DESCRIPTORS;
        }
        if (gArgs.GetBoolArg("-blank", false)) {
            flags |= WALLET_FLAG_BLANK_WALLET;
        }
        std::string format_str = gArgs.GetArg("-format", "sqlite");
        bool force_berkeley = (format_str == "bdb" || format_str == "berkeley" || gArgs.GetBoolArg("-berkeley", false));

        std::shared_ptr<CWallet> wallet = MakeWallet(name, path, flags, force_berkeley);
        if (!wallet) {
            tfm::format(std::cerr, "Failed to create wallet: %s\n", name);
            return false;
        }
        tfm::format(std::cout, "Success: created wallet %s at %s\n", name, path.string());
        tfm::format(std::cout, "Format: %s\n", wallet->GetDatabase().Format());
        tfm::format(std::cout, "To use this wallet, start xpchaind with -wallet=%s\n", name);
        std::cout << std::flush;
        return true;
    }

    if (command == "info") {
        if (!fs::exists(path)) {
            tfm::format(std::cerr, "Error: wallet file does not exist at %s\n", path.string());
            return false;
        }

        std::shared_ptr<CWallet> wallet = CWallet::CreateWalletFromFile(name, path);
        if (!wallet) {
            tfm::format(std::cerr, "Error: failed to load wallet %s\n", name);
            return false;
        }

        LOCK(wallet->cs_wallet);
        fprintf(stdout, "Wallet info\n");
        fprintf(stdout, "===========\n");
        fprintf(stdout, "Name: %s\n", wallet->GetName().c_str());
        fprintf(stdout, "Format: %s\n", wallet->GetDatabase().Format().c_str());
        fprintf(stdout, "Encrypted: %s\n", wallet->IsCrypted() ? "yes" : "no");
        fprintf(stdout, "Descriptors: %s\n", wallet->IsWalletFlagSet(WALLET_FLAG_DESCRIPTORS) ? "yes" : "no");
        fprintf(stdout, "HD (hierarchical deterministic): %s\n", wallet->IsHDEnabled() ? "yes" : "no");
        fprintf(stdout, "Keypool Size: %u\n", (unsigned int)wallet->GetKeyPoolSize());
        fprintf(stdout, "Transactions: %u\n", (unsigned int)wallet->mapWallet.size());
        fprintf(stdout, "Address Book Entries: %u\n", (unsigned int)wallet->mapAddressBook.size());
        fflush(stdout);
        return true;
    }

    if (command == "salvage") {
        std::string filename;
        BerkeleyEnvironment* env = GetWalletEnv(path, filename);
        if (!env) {
            tfm::format(std::cerr, "Failed to get Berkeley DB environment for %s\n", path.string());
            return false;
        }
        std::vector<BerkeleyEnvironment::KeyValPair> salvagedData;
        bool result = env->Salvage(filename, true, salvagedData);
        if (!result) {
            tfm::format(std::cerr, "Salvage failed for %s\n", path.string());
            return false;
        }
        tfm::format(std::cout, "Salvaged %u records from %s\n", salvagedData.size(), path.string());
        std::cout << std::flush;
        return true;
    }

    tfm::format(std::cerr, "Invalid command: %s. Supported commands: info, create, salvage\n", command);
    return false;
}

} // namespace WalletTool
