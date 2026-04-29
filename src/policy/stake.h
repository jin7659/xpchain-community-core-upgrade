// Copyright (c) 2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_POLICY_STAKE_H
#define XPCHAIN_POLICY_STAKE_H

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <consensus/params.h>

bool IsCoinStakeTx(CTransactionRef tx, const Consensus::Params &consensusParams, uint256 &hashBlock, CTransactionRef& prevTx);
bool IsDestinationSame(const CScript& prevTxOut, const CScript& coinStakeTxOut);

#endif //XPCHAIN_POLICY_STAKE_H
