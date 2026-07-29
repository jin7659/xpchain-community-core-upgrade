// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <noui.h>

#include <ui_interface.h>
#include <util.h>

#include <support/allocators/secure.h>

#include <cstdio>
#include <stdint.h>
#include <string>

static bool noui_ThreadSafeMessageBox(const std::string& message, const std::string& caption, unsigned int style)
{
    bool fSecure = style & CClientUIInterface::SECURE;
    style &= ~CClientUIInterface::SECURE;

    std::string strCaption;
    // Check for usage of predefined caption
    switch (style) {
    case CClientUIInterface::MSG_ERROR:
        strCaption += _("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        strCaption += _("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        strCaption += _("Information");
        break;
    default:
        strCaption += caption; // Use supplied caption (can be empty)
    }

    if (!fSecure)
        LogPrintf("%s: %s\n", strCaption, message);
    fprintf(stderr, "%s: %s\n", strCaption.c_str(), message.c_str());
    return false;
}

static bool noui_ThreadSafeQuestion(const std::string& /* ignored interactive message */, const std::string& message, const std::string& caption, unsigned int style)
{
    return noui_ThreadSafeMessageBox(message, caption, style);
}

static bool noui_ThreadSafeAskPassphrase(const std::string& wallet_name, const std::string& message, SecureString& /*passphrase_out*/)
{
    // Daemons cannot prompt interactively; require -walletdbpassphrase.
    LogPrintf("Error: %s (%s)\n", message, wallet_name);
    fprintf(stderr, "Error: %s (%s)\n", message.c_str(), wallet_name.c_str());
    fprintf(stderr, "Error: Set -walletdbpassphrase=<passphrase> on the command line or in xpchain.conf\n");
    return false;
}

static void noui_InitMessage(const std::string& message)
{
    LogPrintf("init message: %s\n", message);
}

void noui_connect()
{
    // Connect xpchaind signal handlers
    uiInterface.ThreadSafeMessageBox.connect(noui_ThreadSafeMessageBox);
    uiInterface.ThreadSafeQuestion.connect(noui_ThreadSafeQuestion);
    uiInterface.ThreadSafeAskPassphrase.connect(noui_ThreadSafeAskPassphrase);
    uiInterface.InitMessage.connect(noui_InitMessage);
}
