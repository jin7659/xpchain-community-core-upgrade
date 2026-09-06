#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reference deposit monitor for XPChain exchange integrations.

Polls listtransactions and emits credits only when:
  - category is 'receive'
  - confirmations >= --minconf
  - address is in the allow-list (file or --address)
  - amount has <= 4 decimal places

It intentionally ignores immature / generate / stake-like categories so
coinstake cannot create false double deposits. This is a teaching /
smoke-test tool, not a production exchange ledger.

Examples:
  ./sample_deposit_monitor.py --datadir ~/.xpchain --address-file deposits.txt --once
  ./sample_deposit_monitor.py --rpcuser u --rpcpassword p --rpcport 18999 \\
      --chain regtest --address xpcrt1q... --minconf 1 --interval 5
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from decimal import Decimal, InvalidOperation
from typing import Dict, Iterable, List, Optional, Set, Tuple

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from rpc import RPCError, connect_from_args  # noqa: E402
from xpc_facts import NETWORKS, amount_to_base_units  # noqa: E402

CREDIT_CATEGORIES = {"receive"}
IGNORE_CATEGORIES = {"immature", "generate", "orphan", "stake"}


def load_addresses(path: Optional[str], extra: List[str]) -> Set[str]:
    addrs: Set[str] = set()
    for a in extra:
        if a.strip():
            addrs.add(a.strip())
    if path:
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    addrs.add(line)
    return addrs


def valid_amount(amount) -> Tuple[bool, Optional[int], str]:
    try:
        # listtransactions returns a JSON number; normalize via Decimal string.
        s = format(Decimal(str(amount)), "f")
        units = amount_to_base_units(s)
        return True, units, s
    except (InvalidOperation, ValueError) as e:
        return False, None, str(e)


def iter_credits(txs: Iterable[dict], allow: Set[str], minconf: int) -> List[dict]:
    out: List[dict] = []
    for tx in txs:
        cat = tx.get("category")
        if cat in IGNORE_CATEGORIES:
            continue
        if cat not in CREDIT_CATEGORIES:
            continue
        conf = int(tx.get("confirmations") or 0)
        if conf < minconf:
            continue
        addr = tx.get("address")
        if allow and addr not in allow:
            continue
        ok, units, normalized = valid_amount(tx.get("amount"))
        if not ok:
            continue
        out.append(
            {
                "txid": tx.get("txid"),
                "vout": tx.get("vout"),
                "address": addr,
                "amount": normalized,
                "amount_base_units": units,
                "confirmations": conf,
                "category": cat,
                "time": tx.get("time"),
            }
        )
    return out


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--chain", choices=sorted(NETWORKS.keys()), default="main")
    parser.add_argument("--rpcconnect", default="127.0.0.1")
    parser.add_argument("--rpcport", type=int)
    parser.add_argument("--rpcuser")
    parser.add_argument("--rpcpassword")
    parser.add_argument("--datadir")
    parser.add_argument("--address", action="append", default=[], help="Allow-listed deposit address (repeatable)")
    parser.add_argument("--address-file", help="File with one deposit address per line")
    parser.add_argument("--minconf", type=int, default=1)
    parser.add_argument("--count", type=int, default=100, help="listtransactions count")
    parser.add_argument("--interval", type=float, default=15.0)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    allow = load_addresses(args.address_file, args.address)
    if not allow:
        print("ERROR: provide --address and/or --address-file", file=sys.stderr)
        return 2

    try:
        rpc = connect_from_args(args)
    except Exception as e:
        print("ERROR: %s" % e, file=sys.stderr)
        return 2

    seen: Set[Tuple[str, int]] = set()
    print(
        "# monitoring %d address(es); minconf=%d; ignore=%s"
        % (len(allow), args.minconf, ",".join(sorted(IGNORE_CATEGORIES))),
        file=sys.stderr,
    )

    while True:
        try:
            txs = rpc.call("listtransactions", ["*", args.count, 0, True])
        except RPCError as e:
            print("RPC error: %s" % e.message, file=sys.stderr)
            if args.once:
                return 1
            time.sleep(args.interval)
            continue

        credits = iter_credits(txs, allow, args.minconf)
        for c in credits:
            key = (str(c["txid"]), int(c.get("vout") or 0))
            if key in seen:
                continue
            seen.add(key)
            if args.json:
                print(json.dumps(c, sort_keys=True))
            else:
                print(
                    "CREDIT %s vout=%s addr=%s amount=%s (%d base) conf=%s"
                    % (
                        c["txid"],
                        c.get("vout"),
                        c["address"],
                        c["amount"],
                        c["amount_base_units"],
                        c["confirmations"],
                    )
                )
            sys.stdout.flush()

        if args.once:
            return 0
        time.sleep(args.interval)


if __name__ == "__main__":
    sys.exit(main())
