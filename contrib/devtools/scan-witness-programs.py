#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain Community developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Find outputs that pay to a witness program of version 1 or higher.

Before the Taproot activation height a node without Taproot support treats a
witness v1 output as an upgrade hook and accepts any spend of it, so such an
output is spendable by anyone. Before treating an activation height as final,
the chain below it has to be checked for outputs that would suddenly become
subject to the Taproot rules -- an output that was never a valid Taproot
commitment becomes unspendable at activation.

Talks to a synced node over JSON-RPC. Does not need -txindex. Reads blocks in
batches, so the node stays responsive; a full mainnet scan takes a while.

Usage:

  ./scan-witness-programs.py --rpcuser u --rpcpassword p --end 4200000
  ./scan-witness-programs.py --datadir ~/.xpchain          # use the auth cookie

Exit status is 0 when no such output exists in the scanned range, 1 when at
least one does, and 2 on error.
'''
import argparse
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.request

# OP_1..OP_16 introduce a witness program of version 1..16.
WITNESS_VERSION_OPCODES = {0x50 + version: version for version in range(1, 17)}


class RPCError(Exception):
    pass


class RPC:
    def __init__(self, url, auth):
        self.url = url
        self.auth = base64.b64encode(auth.encode()).decode()
        self.next_id = 0

    def batch(self, calls):
        '''Send [(method, params), ...] as one JSON-RPC batch, return the results in order.'''
        if not calls:
            return []
        payload = []
        for method, params in calls:
            payload.append({'jsonrpc': '2.0', 'id': self.next_id, 'method': method, 'params': params})
            self.next_id += 1
        request = urllib.request.Request(
            self.url,
            data=json.dumps(payload).encode(),
            headers={'Authorization': 'Basic ' + self.auth, 'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(request) as response:
                body = json.loads(response.read().decode())
        except urllib.error.HTTPError as e:
            raise RPCError('HTTP %d: %s' % (e.code, e.read().decode(errors='replace')))
        except urllib.error.URLError as e:
            raise RPCError('cannot reach %s: %s' % (self.url, e.reason))
        by_id = {entry['id']: entry for entry in body}
        results = []
        for entry_id in [entry['id'] for entry in payload]:
            entry = by_id[entry_id]
            if entry.get('error'):
                raise RPCError(entry['error'].get('message', str(entry['error'])))
            results.append(entry['result'])
        return results

    def call(self, method, *params):
        return self.batch([(method, list(params))])[0]


def witness_version(script_hex):
    '''Witness version of a scriptPubKey, or None when it is not a witness program.

    A witness program is a single version opcode followed by a single push of 2
    to 40 bytes, and nothing else.
    '''
    try:
        script = bytes.fromhex(script_hex)
    except ValueError:
        return None
    if len(script) < 4 or len(script) > 42:
        return None
    version = WITNESS_VERSION_OPCODES.get(script[0])
    if version is None:
        return None
    push_length = script[1]
    if push_length < 2 or push_length > 40 or len(script) != push_length + 2:
        return None
    return version


def read_cookie(datadir, chain_subdir):
    path = os.path.join(os.path.expanduser(datadir), chain_subdir, '.cookie')
    with open(path) as f:
        return f.read().strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--rpcconnect', default='127.0.0.1')
    parser.add_argument('--rpcport', type=int, help='default depends on --chain')
    parser.add_argument('--rpcuser')
    parser.add_argument('--rpcpassword')
    parser.add_argument('--datadir', help='read the auth cookie from here instead of using --rpcuser')
    parser.add_argument('--chain', default='main', choices=['main', 'test', 'regtest'])
    parser.add_argument('--start', type=int, default=0, help='first height to scan (default: 0)')
    parser.add_argument('--end', type=int, help='last height to scan (default: the chain tip)')
    parser.add_argument('--batch', type=int, default=50, help='blocks per JSON-RPC batch (default: 50)')
    args = parser.parse_args()

    default_ports = {'main': 8762, 'test': 18762, 'regtest': 28762}
    cookie_subdirs = {'main': '', 'test': 'testnet', 'regtest': 'regtest'}
    port = args.rpcport if args.rpcport else default_ports[args.chain]

    if args.rpcuser is not None:
        auth = '%s:%s' % (args.rpcuser, args.rpcpassword or '')
    else:
        datadir = args.datadir or '~/.xpchain'
        try:
            auth = read_cookie(datadir, cookie_subdirs[args.chain])
        except OSError as e:
            print('cannot read the auth cookie (%s); pass --rpcuser/--rpcpassword or --datadir' % e,
                  file=sys.stderr)
            return 2

    rpc = RPC('http://%s:%d/' % (args.rpcconnect, port), auth)

    try:
        tip = rpc.call('getblockcount')
        end = args.end if args.end is not None else tip
        if end > tip:
            print('chain tip is %d, scanning only up to there instead of %d' % (tip, end), file=sys.stderr)
            end = tip

        found = []
        started = time.time()
        height = args.start
        while height <= end:
            heights = list(range(height, min(height + args.batch, end + 1)))
            hashes = rpc.batch([('getblockhash', [h]) for h in heights])
            blocks = rpc.batch([('getblock', [h, 2]) for h in hashes])
            for block in blocks:
                for tx in block['tx']:
                    for vout in tx['vout']:
                        version = witness_version(vout['scriptPubKey'].get('hex', ''))
                        if version is None or version < 1:
                            continue
                        found.append((block['height'], tx['txid'], vout['n']))
                        print('height %d tx %s vout %d: witness v%d, %s XPC, %s' % (
                            block['height'], tx['txid'], vout['n'], version,
                            vout['value'], vout['scriptPubKey']['hex']))
            height = heights[-1] + 1
            if height % 10000 < args.batch:
                elapsed = time.time() - started
                scanned = height - args.start
                remaining = (end - height + 1) * elapsed / scanned if scanned else 0
                print('... %d/%d, %d found, %.0f min left' % (
                    height - 1, end, len(found), remaining / 60), file=sys.stderr)

        print('scanned heights %d..%d: %d output(s) paying to a witness program of version >= 1'
              % (args.start, end, len(found)), file=sys.stderr)

        if found:
            print('checking which of them are still unspent', file=sys.stderr)
            outs = rpc.batch([('gettxout', [txid, n]) for _, txid, n in found])
            for (block_height, txid, n), out in zip(found, outs):
                print('%s:%d (height %d) is %s' % (txid, n, block_height,
                                                   'UNSPENT' if out else 'spent'), file=sys.stderr)
            return 1
        return 0
    except RPCError as e:
        print('RPC failed: %s' % e, file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 2


if __name__ == '__main__':
    sys.exit(main())
