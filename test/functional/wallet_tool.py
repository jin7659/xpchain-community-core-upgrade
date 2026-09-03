#!/usr/bin/env python3
# Copyright (c) 2018-2026 The Bitcoin Core developers
# Copyright (c) 2018-2026 The XPChain Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test xpchain-wallet standalone tool."""

import os
import subprocess

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class ToolWalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 0

    def setup_network(self):
        pass

    def wallet_tool_cmd(self, *args):
        binary = os.path.join(os.path.dirname(self.options.bitcoincli), "xpchain-wallet")
        cmd = [binary] + list(args)
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        out, err = process.communicate()
        return process.returncode, out, err

    def run_test(self):
        wallet_dir = os.path.join(self.options.tmpdir, "walletdir")
        os.makedirs(wallet_dir, exist_ok=True)

        self.log.info("Test xpchain-wallet create with default SQLite format")
        ret, out, err = self.wallet_tool_cmd(f"-datadir={wallet_dir}", "-wallet=test_sqlite", "create")
        assert_equal(ret, 0)
        assert "Success: created wallet test_sqlite" in out
        assert "Format: sqlite" in out

        self.log.info("Test xpchain-wallet info on SQLite wallet")
        ret, out, err = self.wallet_tool_cmd(f"-datadir={wallet_dir}", "-wallet=test_sqlite", "info")
        assert_equal(ret, 0)
        assert "Name: test_sqlite" in out
        assert "Format: sqlite" in out
        assert "Encrypted: no" in out
        assert "HD (hierarchical deterministic): yes" in out

        self.log.info("Test xpchain-wallet create with BDB format")
        ret, out, err = self.wallet_tool_cmd(f"-datadir={wallet_dir}", "-wallet=test_bdb", "-format=bdb", "create")
        assert_equal(ret, 0)
        assert "Success: created wallet test_bdb" in out
        assert "Format: berkeley" in out

        self.log.info("Test xpchain-wallet info on BDB wallet")
        ret, out, err = self.wallet_tool_cmd(f"-datadir={wallet_dir}", "-wallet=test_bdb", "info")
        assert_equal(ret, 0)
        assert "Name: test_bdb" in out
        assert "Format: berkeley" in out

        self.log.info("All xpchain-wallet standalone tool tests passed successfully!")


if __name__ == "__main__":
    ToolWalletTest().main()
