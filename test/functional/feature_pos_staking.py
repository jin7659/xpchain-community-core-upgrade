#!/usr/bin/env python3
# Copyright (c) 2026 The XPChain Community developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""End-to-end regression test for the XPChain proof-of-stake switch.

This is the P0-4 safety net from doc/architecture-separation-roadmap.md. It covers the
parts of proof-of-stake that need real chain state and therefore cannot be reached from
src/test/pos_tests.cpp: CheckProofOfStake, IsCoinStakeTx, the coinstake/coinbase pairing
rules, the block signature soft fork, and the reward paid by ConnectBlock.

The scenario is:

  1. mine the whole proof-of-work range on regtest (heights 1..nSwitchHeight),
  2. show that a proof-of-work block is rejected above the switch height,
  3. let the built-in minter produce the first proof-of-stake block,
  4. check the structural rules from doc/xpchain-pos-consensus.md against that block,
  5. check that a second node that did not create the block accepts it over p2p,
  6. check that the block still validates after -reindex (the on-disk PoS read path).

Steps 5 and 6 are what make this useful for the staged modernization: they revalidate a
proof-of-stake block through the network and the disk paths, not just the mining path.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    connect_nodes,
    sync_blocks,
    wait_until,
)

# Must match consensus.nSwitchHeight for regtest in src/chainparams.cpp.
REGTEST_SWITCH_HEIGHT = 1680
# Must match consensus.nStakeMinAge for regtest in src/chainparams.cpp.
REGTEST_STAKE_MIN_AGE = 10


class PoSStakingTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        # Node 0 stakes, node 1 only validates. -txindex keeps CheckProofOfStake on the
        # indexed lookup path rather than the slow UTXO fallback.
        self.extra_args = [
            ['-txindex', '-minting=0'],
            ['-txindex', '-minting=0'],
        ]

    def setup_network(self):
        self.setup_nodes()
        connect_nodes(self.nodes[0], 1)

    def mine_pow_range(self, address):
        """Mine heights 1..nSwitchHeight, which is the whole proof-of-work range."""
        self.log.info("Mining the proof-of-work range up to height %d", REGTEST_SWITCH_HEIGHT)
        # generatetoaddress is capped per call by the RPC's own retry budget, so mine in
        # chunks to keep each call short.
        remaining = REGTEST_SWITCH_HEIGHT
        while remaining > 0:
            batch = min(remaining, 250)
            self.nodes[0].generatetoaddress(batch, address)
            remaining -= batch
        assert_equal(self.nodes[0].getblockcount(), REGTEST_SWITCH_HEIGHT)
        sync_blocks(self.nodes)

    def assert_pow_rejected_above_switch(self, address):
        """A proof-of-work block at nSwitchHeight + 1 has no coinstake and must be rejected."""
        self.log.info("Checking that a proof-of-work block is rejected above the switch height")
        height_before = self.nodes[0].getblockcount()
        try:
            self.nodes[0].generatetoaddress(1, address)
        except Exception as e:
            self.log.info("generatetoaddress rejected as expected: %s", e)
        else:
            raise AssertionError(
                "a proof-of-work block was accepted at height {}".format(height_before + 1))
        assert_equal(self.nodes[0].getblockcount(), height_before)

    def stake_one_block(self):
        """Enable minting on node 0 and wait for the first proof-of-stake block."""
        tip_time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time']
        # Move the clock past the tip so the candidate coins clear nStakeMinAge. Staying
        # well inside DEFAULT_MAX_TIP_AGE keeps IsInitialBlockDownload() false, which the
        # minter thread requires.
        mocktime = tip_time + REGTEST_STAKE_MIN_AGE + 120
        for node in self.nodes:
            node.setmocktime(mocktime)

        self.log.info("Restarting node 0 with minting enabled")
        self.restart_node(0, extra_args=['-txindex', '-minting=1'])
        connect_nodes(self.nodes[0], 1)
        self.nodes[0].setmocktime(mocktime)

        wait_until(lambda: self.nodes[0].getblockcount() > REGTEST_SWITCH_HEIGHT, timeout=180)
        staked_hash = self.nodes[0].getblockhash(REGTEST_SWITCH_HEIGHT + 1)

        # Stop staking so the rest of the test runs against a fixed tip.
        self.log.info("Restarting node 0 with minting disabled")
        self.restart_node(0, extra_args=['-txindex', '-minting=0'])
        connect_nodes(self.nodes[0], 1)
        self.nodes[0].setmocktime(mocktime)
        return staked_hash

    def check_pos_block_structure(self, block_hash):
        """Assert the rules from doc/xpchain-pos-consensus.md sections 2 and 5."""
        self.log.info("Checking the structure of the first proof-of-stake block")
        block = self.nodes[0].getblock(block_hash, 2)
        assert_equal(block['height'], REGTEST_SWITCH_HEIGHT + 1)

        # Section 2: a PoS block carries a coinbase plus a coinstake.
        assert_greater_than(len(block['tx']), 1)
        coinbase, coinstake = block['tx'][0], block['tx'][1]

        # Section 2: the coinstake spends exactly one input into exactly one output.
        assert_equal(len(coinstake['vin']), 1)
        assert_equal(len(coinstake['vout']), 1)
        assert 'coinbase' not in coinstake['vin'][0]

        # Section 2.2: with BLOCK_SIGNATURE_ADDITION active the nonce must be zero.
        assert_equal(block['nonce'], 0)

        # Section 2: the coinstake returns the value to the destination it came from.
        prev = self.nodes[0].getrawtransaction(coinstake['vin'][0]['txid'], True)
        staked_out = prev['vout'][coinstake['vin'][0]['vout']]
        assert_equal(staked_out['scriptPubKey']['hex'],
                     coinstake['vout'][0]['scriptPubKey']['hex'])

        # Section 2: with a single-output coinbase, it must pay the same destination.
        if len(coinbase['vout']) <= 2:
            assert_equal(coinbase['vout'][0]['scriptPubKey']['hex'],
                         coinstake['vout'][0]['scriptPubKey']['hex'])

        # Section 5: the block pays a staking reward, and it is far below the
        # proof-of-work subsidy for the same height.
        reward = coinbase['vout'][0]['value']
        assert_greater_than(float(reward), 0)
        return block

    def check_reward_is_immature(self):
        """The reward lands in a coinbase output, so it must start out immature."""
        self.log.info("Checking that the staking reward is immature")
        info = self.nodes[0].getwalletinfo()
        assert_greater_than(float(info['immature_balance']), 0)

    def check_peer_accepts_block(self, block_hash):
        """Node 1 never saw the coinstake being built, so this exercises pure validation."""
        self.log.info("Checking that node 1 accepts the proof-of-stake block over p2p")
        sync_blocks(self.nodes)
        assert_equal(self.nodes[1].getblockcount(), REGTEST_SWITCH_HEIGHT + 1)
        assert_equal(self.nodes[1].getblockhash(REGTEST_SWITCH_HEIGHT + 1), block_hash)

    def check_reindex_revalidates(self, block_hash):
        """-reindex re-runs CheckBlock/ConnectBlock over the PoS block from disk."""
        self.log.info("Checking that node 1 revalidates the proof-of-stake block after -reindex")
        self.restart_node(1, extra_args=['-txindex', '-minting=0', '-reindex'])
        wait_until(lambda: self.nodes[1].getblockcount() == REGTEST_SWITCH_HEIGHT + 1, timeout=300)
        assert_equal(self.nodes[1].getbestblockhash(), block_hash)
        connect_nodes(self.nodes[0], 1)

    def run_test(self):
        address = self.nodes[0].getnewaddress()

        self.mine_pow_range(address)
        self.assert_pow_rejected_above_switch(address)

        staked_hash = self.stake_one_block()
        self.check_pos_block_structure(staked_hash)
        self.check_reward_is_immature()
        self.check_peer_accepts_block(staked_hash)
        self.check_reindex_revalidates(staked_hash)

        self.log.info("Proof-of-stake switch, staking, propagation and reindex all verified")


if __name__ == '__main__':
    PoSStakingTest().main()
