#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test SQLite wallet encryption and passphrase change (SQLCipher rekey)."""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
from test_framework.test_node import ErrorMatch


class WalletSQLiteEncryptionTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Create SQLite wallet for encryption test")
        node.createwallet("encrypted.sqlite")
        wallet = node.get_wallet_rpc("encrypted.sqlite")

        info = wallet.getwalletinfo()
        assert_equal(info["databaseformat"], "sqlite")

        if not info.get("sqlcipher", False):
            self.log.warning("Skipping at-rest encryption checks: build without SQLCipher")
            return

        self.log.info("Encrypt SQLite wallet and verify at-rest protection")
        passphrase = "sqlite-at-rest-passphrase"
        address = wallet.getnewaddress("", "legacy")
        privkey = wallet.dumpprivkey(address)

        wallet_path = os.path.join(node.datadir, "regtest", "wallets", "encrypted.sqlite")
        assert os.path.isfile(wallet_path)

        with open(wallet_path, "rb") as f:
            plain_header = f.read(16)
        assert plain_header.startswith(b"SQLite format 3"), "New SQLite wallet should start as plain SQLite"

        # encryptwallet shuts down the node after rewriting the wallet file.
        wallet.encryptwallet(passphrase)
        self.wait_for_node_exit(0, timeout=60)

        with open(wallet_path, "rb") as f:
            encrypted_header = f.read(16)
        assert not encrypted_header.startswith(b"SQLite format 3"), "Encrypted wallet file must not be readable as plain SQLite"

        self.log.info("Restart without -walletdbpassphrase must fail (daemon cannot prompt)")
        self.nodes[0].assert_start_raises_init_error(
            ['-wallet=encrypted.sqlite'],
            'requires -walletdbpassphrase',
            match=ErrorMatch.PARTIAL_REGEX,
        )

        self.log.info("Restart node with correct passphrase")
        self.start_node(0, [
            '-walletdbpassphrase=' + passphrase,
            '-wallet=encrypted.sqlite',
        ])
        wallet = node.get_wallet_rpc("encrypted.sqlite")
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase", wallet.dumpprivkey, address)
        wallet.walletpassphrase(passphrase, 60)
        assert_equal(wallet.dumpprivkey(address), privkey)

        # --- walletpassphrasechange + SQLCipher rekey ---
        passphrase2 = "new-at-rest-passphrase"

        self.log.info("Change passphrase (triggers SQLCipher rekey)")
        wallet.walletpassphrasechange(passphrase, passphrase2)

        self.log.info("Verify new passphrase works for app-layer unlock")
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect",
                                wallet.walletpassphrase, passphrase, 10)
        wallet.walletpassphrase(passphrase2, 60)
        assert_equal(wallet.dumpprivkey(address), privkey)
        wallet.walletlock()

        self.log.info("Stop node and verify old passphrase no longer opens the DB file")
        self.stop_node(0)
        self.nodes[0].assert_start_raises_init_error(
            ['-walletdbpassphrase=' + passphrase, '-wallet=encrypted.sqlite'],
            'file is not a database',
            match=ErrorMatch.PARTIAL_REGEX,
        )

        self.log.info("Restart with new passphrase after rekey")
        self.start_node(0, [
            '-walletdbpassphrase=' + passphrase2,
            '-wallet=encrypted.sqlite',
        ])
        wallet = node.get_wallet_rpc("encrypted.sqlite")
        wallet.walletpassphrase(passphrase2, 60)
        assert_equal(wallet.dumpprivkey(address), privkey)

        self.log.info("Confirm wallet file is still not plaintext SQLite")
        self.stop_node(0)
        with open(wallet_path, "rb") as f:
            rekeyed_header = f.read(16)
        assert not rekeyed_header.startswith(b"SQLite format 3"), "Rekeyed wallet must remain encrypted"


if __name__ == '__main__':
    WalletSQLiteEncryptionTest().main()
