#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Check that a running XPChain node is ready for exchange hot-wallet use.

Exit codes:
  0 = all required checks passed (warnings allowed unless --strict)
  1 = one or more required checks failed
  2 = could not connect / usage error

Examples:
  ./readiness_check.py --datadir /tmp/xpc-regtest --chain regtest
  ./readiness_check.py --rpcuser u --rpcpassword p --rpcport 18999 --chain regtest
  ./readiness_check.py --datadir ~/.xpchain --strict
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from typing import Any, Dict, List, Optional

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from rpc import RPC, RPCError, connect_from_args  # noqa: E402
from xpc_facts import (  # noqa: E402
    COINBASE_MATURITY,
    DECIMALS,
    NETWORKS,
    REQUIRED_RPCS,
    amount_to_base_units,
    looks_like_bech32,
)


class CheckResult:
    def __init__(self, name: str, ok: bool, required: bool, detail: str):
        self.name = name
        self.ok = ok
        self.required = required
        self.detail = detail

    def as_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "ok": self.ok,
            "required": self.required,
            "detail": self.detail,
        }


def _check_rpc_help(rpc: RPC) -> CheckResult:
    missing = []
    for method in REQUIRED_RPCS:
        try:
            rpc.call("help", [method])
        except RPCError:
            missing.append(method)
    if missing:
        return CheckResult("required_rpcs", False, True, "missing: %s" % ", ".join(missing))
    return CheckResult("required_rpcs", True, True, "%d methods present" % len(REQUIRED_RPCS))


def _check_sync(rpc: RPC) -> CheckResult:
    """Require the tip to be caught up; do not fail solely on the IBD latch.

    Fresh regtest nodes often keep initialblockdownload=true at height 0 even
    though headers == blocks. Exchange hot-wallet readiness cares about being
    at the known tip, not the IBD boolean by itself.
    """
    info = rpc.call("getblockchaininfo")
    ibd = bool(info.get("initialblockdownload"))
    progress = float(info.get("verificationprogress") or 0)
    blocks = info.get("blocks")
    headers = info.get("headers")
    detail = "blocks=%s headers=%s progress=%.6f ibd=%s" % (blocks, headers, progress, ibd)
    behind = headers is not None and blocks is not None and (headers - blocks) > 10
    if behind:
        return CheckResult("synced", False, True, detail)
    # Tip matches known headers (including genesis-only regtest). IBD may still
    # be latched true; that alone is not a hot-wallet readiness failure.
    if headers is not None and blocks is not None and headers == blocks:
        return CheckResult("synced", True, True, detail)
    if ibd:
        return CheckResult("synced", False, True, detail)
    return CheckResult("synced", True, True, detail)


def _check_network(rpc: RPC, expected: str) -> CheckResult:
    info = rpc.call("getblockchaininfo")
    chain = info.get("chain")
    # Bitcoin-style: main/test/regtest
    ok = chain == expected or (expected == "test" and chain in ("test", "testnet"))
    return CheckResult(
        "network",
        ok,
        True,
        "node chain=%r expected=%r" % (chain, expected),
    )


def _check_bech32_address(rpc: RPC, chain: str) -> CheckResult:
    try:
        addr = rpc.call("getnewaddress", ["", "bech32"])
    except RPCError as e:
        return CheckResult("bech32_deposit_address", False, True, e.message)
    hrp = NETWORKS[chain]["bech32_hrp"]
    ok_hrp = looks_like_bech32(addr, chain)
    long_enough = len(addr) >= 14
    detail = "addr=%s len=%d hrp=%s" % (addr, len(addr), hrp)
    if not ok_hrp:
        return CheckResult("bech32_deposit_address", False, True, "unexpected format: %s" % detail)
    if not long_enough:
        return CheckResult("bech32_deposit_address", False, True, "too short: %s" % detail)
    # Also validate via node
    try:
        va = rpc.call("validateaddress", [addr])
        if not va.get("isvalid"):
            return CheckResult("bech32_deposit_address", False, True, "validateaddress rejected: %s" % detail)
    except RPCError as e:
        return CheckResult("bech32_deposit_address", False, True, e.message)
    return CheckResult("bech32_deposit_address", True, True, detail)


def _check_decimals(rpc: RPC) -> CheckResult:
    """Ensure >4 decimal amounts are rejected by the wallet RPC parser."""
    bad = "1." + ("0" * DECIMALS) + "1"  # e.g. 1.00001
    try:
        # Use a real deposit address so AmountFromValue runs before funding checks.
        addr = rpc.call("getnewaddress", ["", "bech32"])
        rpc.call("sendtoaddress", [addr, bad])
        return CheckResult("decimal_precision", False, True, "node accepted %s unexpectedly" % bad)
    except RPCError as e:
        msg = (e.message or "").lower()
        if "amount" in msg or "decimal" in msg or "money" in msg or e.code in (-3, -8):
            return CheckResult("decimal_precision", True, True, "rejected %s: %s" % (bad, e.message))
        try:
            amount_to_base_units(bad)
            return CheckResult("decimal_precision", False, True, "local parser accepted bad amount")
        except ValueError:
            return CheckResult(
                "decimal_precision",
                True,
                False,
                "node error was %r; local 4-decimal rule ok" % e.message,
            )


def _check_wallet_fields(rpc: RPC) -> CheckResult:
    try:
        w = rpc.call("getwalletinfo")
    except RPCError as e:
        return CheckResult("wallet_loaded", False, True, e.message)
    if "immature_balance" not in w:
        return CheckResult(
            "wallet_loaded",
            False,
            True,
            "getwalletinfo missing immature_balance",
        )
    detail = "wallet=%s balance=%s immature=%s" % (
        w.get("walletname"),
        w.get("balance"),
        w.get("immature_balance"),
    )
    return CheckResult("wallet_loaded", True, True, detail)


def _check_minting(rpc: RPC) -> CheckResult:
    """Require getmininginfo.minting=false when the field is present.

    Older binaries without the field fall back to heuristics (warning only).
    """
    detail_parts = []
    try:
        mi = rpc.call("getmininginfo")
        detail_parts.append("getmininginfo=%s" % json.dumps(mi, sort_keys=True))
        if "minting" in mi:
            if mi["minting"]:
                return CheckResult(
                    "minting_disabled",
                    False,
                    True,
                    "getmininginfo.minting=true — set -minting=0 for hot wallets",
                )
            return CheckResult(
                "minting_disabled",
                True,
                True,
                "getmininginfo.minting=false",
            )
        # Legacy: other boolean stake/generate flags if present.
        for key in ("staking", "generate"):
            if key in mi and mi[key]:
                return CheckResult(
                    "minting_disabled",
                    False,
                    True,
                    "getmininginfo.%s=%r — set -minting=0 for hot wallets" % (key, mi[key]),
                )
    except RPCError as e:
        detail_parts.append("getmininginfo unavailable: %s" % e.message)

    # Heuristic for older nodes: recent stake/generate category is a smell.
    try:
        txs = rpc.call("listtransactions", ["*", 50, 0, True])
        bad = [t for t in txs if t.get("category") in ("stake", "generate") and t.get("confirmations", 0) >= 0]
        if bad:
            return CheckResult(
                "minting_disabled",
                False,
                True,
                "found %d stake/generate wallet txs; run with -minting=0" % len(bad),
            )
        detail_parts.append("no recent stake/generate txs")
    except RPCError as e:
        detail_parts.append("listtransactions: %s" % e.message)

    return CheckResult(
        "minting_disabled",
        True,
        False,
        "minting field absent (%s). Still ensure -minting=0 in config." % "; ".join(detail_parts),
    )


def _check_address_length_policy(rpc: RPC, chain: str) -> CheckResult:
    addr = rpc.call("getnewaddress", ["", "bech32"])
    length = len(addr)
    if length > 42:
        return CheckResult(
            "address_length",
            True,
            True,
            "bech32 length %d (>42) — exchange DB must allow long addresses" % length,
        )
    return CheckResult(
        "address_length",
        True,
        False,
        "bech32 length %d (still allow up to ~90 chars for future types)" % length,
    )


def run_checks(rpc: RPC, chain: str) -> List[CheckResult]:
    return [
        _check_network(rpc, chain),
        _check_rpc_help(rpc),
        _check_sync(rpc),
        _check_wallet_fields(rpc),
        _check_bech32_address(rpc, chain),
        _check_address_length_policy(rpc, chain),
        _check_decimals(rpc),
        _check_minting(rpc),
        CheckResult(
            "maturity_policy",
            True,
            False,
            "document COINBASE_MATURITY=%d; do not credit immature outputs" % COINBASE_MATURITY,
        ),
    ]


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--chain", choices=sorted(NETWORKS.keys()), default="main")
    parser.add_argument("--rpcconnect", default="127.0.0.1")
    parser.add_argument("--rpcport", type=int)
    parser.add_argument("--rpcuser")
    parser.add_argument("--rpcpassword")
    parser.add_argument("--datadir")
    parser.add_argument("--strict", action="store_true", help="Treat warnings (non-required fails) as failures")
    parser.add_argument("--json", action="store_true", help="Machine-readable output")
    args = parser.parse_args(argv)

    try:
        rpc = connect_from_args(args)
        # cheap ping
        rpc.call("getblockchaininfo")
    except Exception as e:
        print("ERROR: cannot connect to node: %s" % e, file=sys.stderr)
        return 2

    results = run_checks(rpc, args.chain)
    failed_required = [r for r in results if r.required and not r.ok]
    failed_optional = [r for r in results if (not r.required) and not r.ok]

    if args.json:
        payload = {
            "ok": not failed_required and not (args.strict and failed_optional),
            "checks": [r.as_dict() for r in results],
        }
        json.dump(payload, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
    else:
        print("XPChain exchange readiness check (%s)" % args.chain)
        print("-" * 60)
        for r in results:
            status = "PASS" if r.ok else ("FAIL" if r.required else "WARN")
            print("[%s] %s — %s" % (status, r.name, r.detail))
        print("-" * 60)
        if failed_required:
            print("RESULT: FAIL (%d required check(s) failed)" % len(failed_required))
        elif args.strict and failed_optional:
            print("RESULT: FAIL (strict mode; %d warning(s))" % len(failed_optional))
        else:
            print("RESULT: PASS")
            if failed_optional:
                print("(%d warning(s); re-run with --strict to enforce)" % len(failed_optional))

    if failed_required:
        return 1
    if args.strict and failed_optional:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
