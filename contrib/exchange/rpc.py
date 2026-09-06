#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Minimal JSON-RPC helper for XPChain exchange tooling (stdlib only)."""

from __future__ import annotations

import base64
import json
import os
import urllib.error
import urllib.request
from typing import Any, List, Optional, Sequence, Tuple


class RPCError(Exception):
    def __init__(self, code: Optional[int], message: str):
        super().__init__(message)
        self.code = code
        self.message = message


class RPC:
    def __init__(self, url: str, auth: Optional[str] = None, timeout: float = 30.0):
        self.url = url
        self.auth = auth
        self.timeout = timeout
        self._next_id = 1

    def call(self, method: str, params: Optional[Sequence[Any]] = None) -> Any:
        results = self.batch([(method, list(params or []))])
        return results[0]

    def batch(self, calls: Sequence[Tuple[str, Sequence[Any]]]) -> List[Any]:
        payload = []
        for method, params in calls:
            payload.append(
                {
                    "jsonrpc": "1.0",
                    "id": self._next_id,
                    "method": method,
                    "params": list(params),
                }
            )
            self._next_id += 1
        data = json.dumps(payload if len(payload) > 1 else payload[0]).encode("utf-8")
        headers = {"Content-Type": "application/json"}
        if self.auth is not None:
            token = base64.b64encode(self.auth.encode("utf-8")).decode("ascii")
            headers["Authorization"] = "Basic " + token
        req = urllib.request.Request(self.url, data=data, headers=headers, method="POST")
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                body = resp.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            detail = e.read().decode("utf-8", errors="replace")
            raise RPCError(e.code, "HTTP %s: %s" % (e.code, detail)) from e
        except urllib.error.URLError as e:
            raise RPCError(None, "cannot reach %s: %s" % (self.url, e.reason)) from e

        parsed = json.loads(body)
        entries = parsed if isinstance(parsed, list) else [parsed]
        out: List[Any] = []
        for entry in entries:
            if entry.get("error"):
                err = entry["error"]
                raise RPCError(err.get("code"), err.get("message", str(err)))
            out.append(entry.get("result"))
        return out


def read_cookie(datadir: str, chain: str = "main") -> str:
    """Return 'user:password' from the node cookie file."""
    sub = {"": "", "main": "", "test": "testnet3", "testnet": "testnet3", "regtest": "regtest"}
    # XPChain uses "testnet3" directory name like Bitcoin for -testnet.
    rel = sub.get(chain, chain)
    path = os.path.join(datadir, rel, ".cookie") if rel else os.path.join(datadir, ".cookie")
    # Also try without network subdir when chain is main and cookie is at root.
    candidates = [path]
    if chain in ("main", ""):
        candidates.append(os.path.join(datadir, ".cookie"))
    if chain in ("test", "testnet"):
        candidates.append(os.path.join(datadir, "testnet3", ".cookie"))
        candidates.append(os.path.join(datadir, "testnet", ".cookie"))
    if chain == "regtest":
        candidates.append(os.path.join(datadir, "regtest", ".cookie"))
    last_err: Optional[Exception] = None
    for candidate in candidates:
        try:
            with open(candidate, "r", encoding="utf-8") as f:
                return f.read().strip()
        except OSError as e:
            last_err = e
    raise FileNotFoundError("cookie not found under %s (last error: %s)" % (datadir, last_err))


def connect_from_args(args: Any) -> RPC:
    """Build an RPC client from argparse namespace with common flags."""
    host = getattr(args, "rpcconnect", "127.0.0.1")
    port = getattr(args, "rpcport", None)
    chain = getattr(args, "chain", "main")
    if port is None:
        from xpc_facts import NETWORKS

        port = NETWORKS[chain]["rpc_port"]
    auth = None
    if getattr(args, "rpcuser", None) is not None:
        auth = "%s:%s" % (args.rpcuser, getattr(args, "rpcpassword", "") or "")
    elif getattr(args, "datadir", None):
        auth = read_cookie(args.datadir, chain)
    url = "http://%s:%d/" % (host, int(port))
    return RPC(url, auth)
