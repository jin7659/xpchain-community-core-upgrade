#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test mnemonic import rescan behaviour and balance recovery."""

import os
import shutil

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
)

TEST_MNEMONIC = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"


def get_bip44_external_addresses(node, coin_type=0):
    addresses = node.getaddressesbylabel("")
    external = []
    prefix = f"m/44'/{coin_type}'/0'/0/"
    for address in addresses:
        info = node.getaddressinfo(address)
        path = info.get("hdkeypath", "")
        if path.startswith(prefix):
            external.append(address)
    external.sort(key=lambda a: int(node.getaddressinfo(a)["hdkeypath"].rsplit("/", 1)[-1]))
    return external


class WalletMnemonicRescanTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], []]

    def run_test(self):
        self.log.info("Prepare funded BIP44 address via reference wallet")
        self.nodes[0].generate(101)

        self.nodes[1].importmnemonic(
            TEST_MNEMONIC,
            {"bip44": True, "bip44_coin_type": 0, "gap_limit": 5, "rescan": False},
        )
        funded_address = get_bip44_external_addresses(self.nodes[1], coin_type=0)[0]
        self.nodes[0].sendtoaddress(funded_address, 2)
        self.nodes[0].generate(1)
        self.sync_all()

        self.log.info("Rescan on existing wallet picks up funded UTXO")
        self.nodes[1].rescanblockchain(0)
        assert_equal(self.nodes[1].getbalance(), 2)

        self.log.info("Fresh wallet import with rescan=true restores balance")
        wallet_dir = os.path.join(self.nodes[1].datadir, "regtest", "wallets")
        backup_path = os.path.join(self.nodes[1].datadir, "recovery_wallet.bak")
        shutil.copyfile(os.path.join(wallet_dir, "wallet.dat"), backup_path)

        self.stop_node(1)
        shutil.rmtree(os.path.join(self.nodes[1].datadir, "regtest", "wallets"))
        os.makedirs(wallet_dir, exist_ok=True)
        self.start_node(1)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()

        assert_equal(self.nodes[1].getbalance(), 0)

        result = self.nodes[1].importmnemonic(
            TEST_MNEMONIC,
            {"bip44": True, "bip44_coin_type": 0, "gap_limit": 5, "rescan": True},
        )
        assert_equal(result["success"], True)
        assert_equal(result["rescan"], True)
        assert_equal(self.nodes[1].getbalance(), 2)

        self.log.info("Import without rescan then manual rescan also works")
        self.stop_node(1)
        shutil.rmtree(os.path.join(self.nodes[1].datadir, "regtest", "wallets"))
        os.makedirs(wallet_dir, exist_ok=True)
        self.start_node(1)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()

        result = self.nodes[1].importmnemonic(
            TEST_MNEMONIC,
            {"bip44": True, "bip44_coin_type": 0, "gap_limit": 5, "rescan": False},
        )
        assert_equal(result["rescan"], False)
        assert_equal(self.nodes[1].getbalance(), 0)

        scan_result = self.nodes[1].rescanblockchain(0)
        assert_equal(scan_result["start_height"], 0)
        assert_equal(self.nodes[1].getbalance(), 2)


if __name__ == '__main__':
    WalletMnemonicRescanTest().main()
