// Copyright (c) 2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_POS_STAKE_POLICY_H
#define XPCHAIN_POS_STAKE_POLICY_H

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <consensus/params.h>

bool IsCoinStakeTx(CTransactionRef tx, const Consensus::Params &consensusParams, uint256 &hashBlock, CTransactionRef& prevTx);
bool IsDestinationSame(const CScript& prevTxOut, const CScript& coinStakeTxOut);

#endif //XPCHAIN_POS_STAKE_POLICY_H
