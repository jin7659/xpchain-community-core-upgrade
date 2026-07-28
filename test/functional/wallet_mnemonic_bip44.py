#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test BIP39/BIP44 mnemonic import and coin_type compatibility."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes_bi,
)

# BIP39 test vector (12 words, empty passphrase).
TEST_MNEMONIC = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
INVALID_MNEMONIC = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon"


def get_bip44_external_addresses(node, coin_type=0):
    """Return imported BIP44 external-chain addresses (label='' in address book)."""
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


class WalletMnemonicBIP44Test(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [[], [], []]

    def run_test(self):
        self.log.info("Mine blocks for funding")
        self.nodes[0].generate(101)
        self.sync_all()

        self.log.info("Reject invalid mnemonic checksum")
        assert_raises_rpc_error(
            -8,
            "Mnemonic checksum validation failed",
            self.nodes[1].importmnemonic,
            INVALID_MNEMONIC,
            {"bip44": True, "rescan": False},
        )

        self.log.info("Import BIP44 mnemonic with coin_type=0 (current web wallet)")
        result = self.nodes[1].importmnemonic(
            TEST_MNEMONIC,
            {"bip44": True, "bip44_coin_type": 0, "gap_limit": 10, "rescan": False},
        )
        assert_equal(result["success"], True)
        assert_equal(result["bip44"], True)
        assert_equal(result["bip44_coin_type"], 0)
        assert_equal(result["rescan"], False)

        coin_type_0_addresses = get_bip44_external_addresses(self.nodes[1], coin_type=0)
        assert len(coin_type_0_addresses) >= 10

        self.log.info("Import same mnemonic with coin_type=398 (legacy web wallet)")
        result = self.nodes[2].importmnemonic(
            TEST_MNEMONIC,
            {"bip44": True, "bip44_coin_type": 398, "gap_limit": 10, "rescan": False},
        )
        assert_equal(result["success"], True)
        assert_equal(result["bip44_coin_type"], 398)

        coin_type_398_addresses = get_bip44_external_addresses(self.nodes[2], coin_type=398)
        assert len(coin_type_398_addresses) >= 10
        assert coin_type_0_addresses[0] != coin_type_398_addresses[0]

        self.log.info("Fund the coin_type=0 external address")
        target_address = coin_type_0_addresses[0]
        self.nodes[0].sendtoaddress(target_address, 1)
        self.nodes[0].generate(1)
        self.sync_all()

        self.log.info("Rescan coin_type=0 wallet and verify balance")
        self.nodes[1].rescanblockchain(0)
        assert_equal(self.nodes[1].getbalance(), 1)

        self.log.info("Legacy coin_type=398 wallet must not see the same UTXO")
        self.nodes[2].rescanblockchain(0)
        assert_equal(self.nodes[2].getbalance(), 0)

        self.log.info("Verify imported address hdkeypath reflects coin_type")
        info_0 = self.nodes[1].getaddressinfo(coin_type_0_addresses[0])
        info_398 = self.nodes[2].getaddressinfo(coin_type_398_addresses[0])
        assert_equal(info_0["hdkeypath"], "m/44'/0'/0'/0/0")
        assert_equal(info_398["hdkeypath"], "m/44'/398'/0'/0/0")


if __name__ == '__main__':
    WalletMnemonicBIP44Test().main()
