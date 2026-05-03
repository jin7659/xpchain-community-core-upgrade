// Copyright (c) 2017-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/walletutil.h>

fs::path GetWalletDir()
{
    fs::path path;

    if (gArgs.IsArgSet("-walletdir")) {
        path = gArgs.GetArg("-walletdir", "");
        if (!fs::is_directory(path)) {
            // If the path specified doesn't exist, we return the deliberately
            // invalid empty string.
            path = "";
        }
    } else {
        fs::path datadir = GetDataDir();
        // If 'wallets' directory exists, or if there is no 'wallet.dat' in the root,
        // we use and ensure the 'wallets' subdirectory.
        if (fs::is_directory(datadir / "wallets") || !fs::exists(datadir / "wallet.dat")) {
            path = datadir / "wallets";
            if (!fs::exists(path)) {
                fs::create_directories(path);
            }
        } else {
            path = datadir;
        }
    }

    return path;
}
