#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Berkeley DB to SQLite wallet migration."""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class WalletMigrateSQLiteTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.supports_cli = True

    def run_test(self):
        node = self.nodes[0]
        wallet_dir = os.path.join(node.datadir, "regtest", "wallets")

        self.log.info("Create legacy Berkeley DB wallet (non-wallet.dat name)")
        # createwallet defaults to SQLite; pass berkeley=true for a BDB source wallet.
        cw_res = node.createwallet("legacy_bdb", False, False, "", False, True)
        assert "warning" in cw_res
        assert "deprecated" in cw_res["warning"]
        legacy = node.get_wallet_rpc("legacy_bdb")

        self.log.info("Fund legacy BDB wallet")
        addr = legacy.getnewaddress()
        node.generatetoaddress(101, addr)
        expected_balance = legacy.getbalance()
        assert expected_balance > 0

        legacy_path = os.path.join(wallet_dir, "legacy_bdb", "wallet.dat")
        assert os.path.isfile(legacy_path), "Expected Berkeley DB wallet file"
        assert_equal(legacy.getwalletinfo()["databaseformat"], "berkeley")

        self.log.info("Migrate legacy wallet to SQLite")
        result = legacy.migratewallet({"backup": True, "load_new": True})
        assert_equal(result["success"], True)
        assert result["destination"].endswith(".sqlite")
        assert result["records_copied"] > 0
        assert os.path.isfile(result["destination"])
        assert os.path.isfile(result["backup"])

        migrated = node.get_wallet_rpc(result["loaded_wallet"])
        assert_equal(migrated.getbalance(), expected_balance)

        self.log.info("Verify migrated wallet can send")
        default = node.get_wallet_rpc("")
        send_addr = default.getnewaddress()
        txid = migrated.sendtoaddress(send_addr, 1)
        assert txid

        self.log.info("Reject migration on already-loaded SQLite wallet")
        assert_raises_rpc_error(
            -4,
            "not a Berkeley DB wallet",
            migrated.migratewallet,
        )

        self.log.info("Reject duplicate migration to existing destination")
        node.loadwallet("legacy_bdb")
        legacy_again = node.get_wallet_rpc("legacy_bdb")
        assert_raises_rpc_error(
            -4,
            "Destination wallet already exists",
            legacy_again.migratewallet,
            {"destination": os.path.basename(result["destination"])},
        )


if __name__ == '__main__':
    WalletMigrateSQLiteTest().main()
