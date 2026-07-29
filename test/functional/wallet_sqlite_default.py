#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that newly created wallets default to SQLite while existing BDB wallets still open."""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class WalletSQLiteDefaultTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.supports_cli = True

    def run_test(self):
        node = self.nodes[0]
        wallet_dir = os.path.join(node.datadir, "regtest", "wallets")

        self.log.info("Default wallet should use SQLite")
        default_info = node.getwalletinfo()
        assert_equal(default_info["databaseformat"], "sqlite")
        default_path = os.path.join(wallet_dir, "wallet.dat")
        assert os.path.isfile(default_path)

        self.log.info("createwallet without berkeley flag creates SQLite")
        node.createwallet("modern")
        modern = node.get_wallet_rpc("modern")
        assert_equal(modern.getwalletinfo()["databaseformat"], "sqlite")
        modern_path = os.path.join(wallet_dir, "modern", "wallet.dat")
        assert os.path.isfile(modern_path)

        self.log.info("createwallet berkeley=true still creates BDB for migration/legacy use")
        # positional: name, disable_private_keys, descriptors, passphrase, load_on_startup, berkeley
        node.createwallet("legacy_bdb", False, False, "", False, True)
        legacy = node.get_wallet_rpc("legacy_bdb")
        assert_equal(legacy.getwalletinfo()["databaseformat"], "berkeley")
        legacy_path = os.path.join(wallet_dir, "legacy_bdb", "wallet.dat")
        assert os.path.isfile(legacy_path)

        self.log.info("Existing BDB wallet remains usable after unload/load")
        addr = legacy.getnewaddress()
        node.generatetoaddress(1, addr)
        node.unloadwallet("legacy_bdb")
        node.loadwallet("legacy_bdb")
        legacy_again = node.get_wallet_rpc("legacy_bdb")
        assert_equal(legacy_again.getwalletinfo()["databaseformat"], "berkeley")


if __name__ == '__main__':
    WalletSQLiteDefaultTest().main()
