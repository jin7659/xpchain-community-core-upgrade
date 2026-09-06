#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Offline self-tests for contrib/exchange helpers (no running node required)."""

from __future__ import annotations

import os
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from sample_deposit_monitor import iter_credits  # noqa: E402
from xpc_facts import (  # noqa: E402
    COIN,
    DECIMALS,
    NETWORKS,
    amount_to_base_units,
    base_units_to_amount,
    listing_fact_sheet,
    looks_like_bech32,
)


class AmountTests(unittest.TestCase):
    def test_roundtrip(self):
        self.assertEqual(amount_to_base_units("1"), COIN)
        self.assertEqual(amount_to_base_units("1.2345"), 12345)
        self.assertEqual(amount_to_base_units("0.0001"), 1)
        self.assertEqual(base_units_to_amount(12345), "1.2345")
        self.assertEqual(base_units_to_amount(COIN), "1")

    def test_too_many_decimals(self):
        with self.assertRaises(ValueError):
            amount_to_base_units("1.23456")

    def test_decimals_constant(self):
        self.assertEqual(DECIMALS, 4)
        self.assertEqual(COIN, 10_000)


class AddressTests(unittest.TestCase):
    def test_hrps(self):
        self.assertEqual(NETWORKS["main"]["bech32_hrp"], "xpc")
        self.assertEqual(NETWORKS["test"]["bech32_hrp"], "txpc")
        self.assertEqual(NETWORKS["regtest"]["bech32_hrp"], "xpcrt")

    def test_looks_like_bech32(self):
        # Synthetic but charset-valid payloads (checksum not verified here).
        self.assertTrue(looks_like_bech32("xpc1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq", "main"))
        self.assertFalse(looks_like_bech32("xp1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq", "main"))
        self.assertFalse(looks_like_bech32("bc1qar0srrr7xfkvy5l643lydnw9re59gtzzwf5mdq", "main"))


class FactSheetTests(unittest.TestCase):
    def test_sheet_keys(self):
        sheet = listing_fact_sheet("main")
        self.assertEqual(sheet["units"]["decimals"], 4)
        self.assertEqual(sheet["addresses"]["bech32_hrp"], "xpc")
        self.assertEqual(sheet["network"]["p2p_port"], 8798)
        self.assertEqual(sheet["network"]["rpc_port"], 8762)


class DepositFilterTests(unittest.TestCase):
    def test_filters(self):
        allow = {"xpc1qdeposit0000000000000000000000000000000000000000000"}
        txs = [
            {
                "category": "receive",
                "confirmations": 6,
                "address": list(allow)[0],
                "amount": 1.25,
                "txid": "aa",
                "vout": 0,
            },
            {
                "category": "immature",
                "confirmations": 1,
                "address": list(allow)[0],
                "amount": 50,
                "txid": "bb",
                "vout": 0,
            },
            {
                "category": "receive",
                "confirmations": 6,
                "address": "xpc1qother",
                "amount": 2,
                "txid": "cc",
                "vout": 0,
            },
            {
                "category": "generate",
                "confirmations": 100,
                "address": list(allow)[0],
                "amount": 50,
                "txid": "dd",
                "vout": 0,
            },
        ]
        credits = iter_credits(txs, allow, minconf=1)
        self.assertEqual(len(credits), 1)
        self.assertEqual(credits[0]["txid"], "aa")
        self.assertEqual(credits[0]["amount_base_units"], 12500)


if __name__ == "__main__":
    unittest.main()
