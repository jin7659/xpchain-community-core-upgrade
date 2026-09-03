// Copyright (c) 2016-2026 The Bitcoin Core developers
// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/xpchain-config.h>
#endif

#include <chainparams.h>
#include <chainparamsbase.h>
#include <clientversion.h>
#include <key.h>
#include <pubkey.h>
#include <util.h>
#include <utilstrencodings.h>
#include <wallet/wallettool.h>

#include <stdio.h>
#include <string>

static void SetupWalletToolArgs()
{
    SetupChainParamsBaseOptions();

    gArgs.AddArg("-?", "This help message", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-wallet=<path>", "Specify wallet path or name (default: wallet.dat)", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-format=<format>", "Specify wallet database format for creation: sqlite (default) or bdb", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-descriptors", "Create descriptor wallet (default: false)", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-blank", "Create blank wallet without keys (default: false)", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("-datadir=<dir>", "Specify data directory", false, OptionsCategory::OPTIONS);

    gArgs.AddArg("info", "Show wallet information and metadata", false, OptionsCategory::COMMANDS);
    gArgs.AddArg("create", "Create a new wallet file", false, OptionsCategory::OPTIONS);
    gArgs.AddArg("salvage", "Attempt to recover keys from a corrupted Berkeley DB wallet", false, OptionsCategory::COMMANDS);
}

int main(int argc, char* argv[])
{
    SetupEnvironment();
    SetupWalletToolArgs();
    ECC_Start();
    ECCVerifyHandle globalVerifyHandle;

    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        tfm::format(std::cerr, "Error parsing command line arguments: %s\n", error.c_str());
        return EXIT_FAILURE;
    }

    if (HelpRequested(gArgs) || gArgs.GetArgs("-?").size()) {
        std::string strUsage = strprintf("%s xpchain-wallet version", PACKAGE_NAME) + " " + FormatFullVersion() + "\n\n" +
                               "Usage:  xpchain-wallet [options] <command> [wallet_name]\n\n" +
                               "Commands:\n" +
                               "  info     Get wallet info\n" +
                               "  create   Create new wallet\n" +
                               "  salvage  Salvage corrupted Berkeley DB wallet\n\n" +
                               gArgs.GetHelpMessage();
        tfm::format(std::cout, "%s", strUsage.c_str());
        return EXIT_SUCCESS;
    }

    // Extract command and wallet name
    std::string command;
    std::string name = gArgs.GetArg("-wallet", "wallet.dat");

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!arg.empty() && arg[0] != '-') {
            if (command.empty()) {
                command = arg;
            } else if (name == "wallet.dat" && !gArgs.IsArgSet("-wallet")) {
                name = arg;
            }
        }
    }

    if (command.empty()) {
        tfm::format(std::cerr, "Error: No command specified. Available commands: info, create, salvage\n");
        tfm::format(std::cerr, "Use 'xpchain-wallet -?' for available options.\n");
        return EXIT_FAILURE;
    }

    try {
        SelectParams(gArgs.GetChainName());
    } catch (const std::exception& e) {
        tfm::format(std::cerr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    bool success = WalletTool::ExecuteWalletToolFunc(command, name);
    ECC_Stop();
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
