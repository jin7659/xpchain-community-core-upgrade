#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Exchange hot-wallet readiness regression test.

Verifies the operator-facing contract used by contrib/exchange/:
  - getmininginfo.minting reflects -minting
  - readiness_check.py passes with -minting=0 and fails with -minting=1
  - bech32 deposit addresses use the regtest HRP and are long enough
  - amounts with more than 4 decimals are rejected
  - sample_deposit_monitor filters immature/generate/stake categories
"""

import json
import os
import subprocess
import sys

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    get_auth_cookie,
    rpc_port,
)

EXCHANGE_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "contrib", "exchange")
)


class ExchangeHotWalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        # Default node is exchange-safe: minting off.
        self.extra_args = [["-minting=0"]]

    def run_readiness(self, node, expect_ok):
        # TestNode has no .rpc_port attribute; node.rpc_port would hit __getattr__
        # and become an AuthServiceProxyWrapper. Use util.rpc_port(index) instead.
        rpc_user, rpc_password = get_auth_cookie(node.datadir)
        cmd = [
            sys.executable,
            os.path.join(EXCHANGE_DIR, "readiness_check.py"),
            "--chain",
            "regtest",
            "--rpcconnect",
            "127.0.0.1",
            "--rpcport",
            str(rpc_port(node.index)),
            "--rpcuser",
            rpc_user,
            "--rpcpassword",
            rpc_password,
            "--json",
            "--strict",
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.log.info("readiness stdout: %s", proc.stdout.strip())
        if proc.stderr:
            self.log.info("readiness stderr: %s", proc.stderr.strip())
        if expect_ok:
            assert_equal(proc.returncode, 0)
            payload = json.loads(proc.stdout)
            assert_equal(payload["ok"], True)
            minting = [c for c in payload["checks"] if c["name"] == "minting_disabled"][0]
            assert_equal(minting["ok"], True)
            assert_equal(minting["required"], True)
        else:
            assert_equal(proc.returncode, 1)
            payload = json.loads(proc.stdout)
            assert_equal(payload["ok"], False)

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Mine one block so tip state is non-IBD-ambiguous")
        node.generate(1)

        self.log.info("getmininginfo.minting is false when started with -minting=0")
        mi = node.getmininginfo()
        assert "minting" in mi
        assert_equal(mi["minting"], False)

        self.log.info("bech32 deposit address uses xpcrt HRP and is long")
        addr = node.getnewaddress("", "bech32")
        assert addr.startswith("xpcrt1")
        assert_greater_than(len(addr), 42)
        assert_equal(node.validateaddress(addr)["isvalid"], True)

        self.log.info("amounts with >4 decimals are rejected")
        assert_raises_rpc_error(-3, "Invalid amount", node.sendtoaddress, addr, "1.00001")

        self.log.info("readiness_check passes for minting=0 hot wallet")
        self.run_readiness(node, expect_ok=True)

        self.log.info("sample_deposit_monitor filters non-receive categories")
        sys.path.insert(0, EXCHANGE_DIR)
        from sample_deposit_monitor import iter_credits

        allow = {addr}
        txs = [
            {
                "category": "receive",
                "confirmations": 6,
                "address": addr,
                "amount": 1.25,
                "txid": "recv",
                "vout": 0,
            },
            {
                "category": "immature",
                "confirmations": 1,
                "address": addr,
                "amount": 50,
                "txid": "imm",
                "vout": 0,
            },
            {
                "category": "stake",
                "confirmations": 10,
                "address": addr,
                "amount": 0.5,
                "txid": "stk",
                "vout": 1,
            },
            {
                "category": "generate",
                "confirmations": 100,
                "address": addr,
                "amount": 50,
                "txid": "gen",
                "vout": 0,
            },
        ]
        credits = iter_credits(txs, allow, minconf=1)
        assert_equal(len(credits), 1)
        assert_equal(credits[0]["txid"], "recv")

        self.log.info("restart with -minting=1; readiness must fail")
        self.restart_node(0, ["-minting=1"])
        mi = node.getmininginfo()
        assert_equal(mi["minting"], True)
        self.run_readiness(node, expect_ok=False)


if __name__ == "__main__":
    ExchangeHotWalletTest().main()
