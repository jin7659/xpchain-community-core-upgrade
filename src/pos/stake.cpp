// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/stake.h>

#include <consensus/merkle.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <key_io.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <util.h>
#include <utilstrencodings.h>
#include <validation.h>

#include <vector>

namespace pos {

bool IsDestinationSame(const CScript& a, const CScript& b)
{
    txnouttype aType, bType;
    std::vector<std::vector<unsigned char>> aSol, bSol;

    if (!Solver(a, aType, aSol) || !Solver(b, bType, bSol)) {
        return false;
    }

    if (aSol.size() != 1 || bSol.size() != 1) {
        return false;
    }

    CTxDestination aDest, bDest;
    if (!ExtractDestination(a, aDest) || !ExtractDestination(b, bDest)) {
        return false;
    }

    if (!(aDest == bDest)) {
        return false;
    }

    return true;
}

bool IsCoinStakeTx(CTransactionRef tx, const Consensus::Params &consensusParams, uint256 &hashBlock,
                   CTransactionRef& prevTx)
{
    if (tx->vin.size() != 1) {
        return error("%s: coinstake has too many inputs", __func__);
    }
    if (tx->vout.size() != 1) {
        return error("%s: coinstake has too many outputs", __func__);
    }

    if (!GetTransaction(tx->vin[0].prevout.hash, prevTx, consensusParams, hashBlock, true)) {
        return error("%s: unknown coinstake input", __func__);
    }

    if (prevTx->GetHash() != tx->vin[0].prevout.hash) {
        return error("%s: invalid coinstake input hash", __func__);
    }

    if (!pos::IsDestinationSame(prevTx->vout[tx->vin[0].prevout.n].scriptPubKey, tx->vout[0].scriptPubKey)) {
        return error("%s: invalid coinstake output", __func__);
    }

    return true;
}

static bool GetPubKeyFromScript(CScript scriptPubKey, const CTxIn& txIn, std::vector<CPubKey>& vPubKey, int depth = 0)
{
    assert(depth <= 2);
    txnouttype type;
    std::vector<std::vector<unsigned char>> vSolutions;
    if (!Solver(scriptPubKey, type, vSolutions)) {
        return false;
    }
    vPubKey.clear();
    switch (type) {
        case TX_PUBKEY:
            vPubKey.push_back(CPubKey(vSolutions[0].begin(), vSolutions[0].end()));
            break;
        case TX_MULTISIG:
            for (auto itr = vSolutions.begin() + 1; itr != vSolutions.end() - 1; itr++) {
                vPubKey.push_back(CPubKey(itr->begin(), itr->end()));
            }
            break;
        case TX_PUBKEYHASH:
        {
            std::vector<std::vector<unsigned char>> stack;
            if (!EvalScript(stack, txIn.scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker(), SigVersion::BASE)) {
                return false;
            }
            vPubKey.push_back(CPubKey(stack.back().begin(), stack.back().end()));
        }
        break;
        case TX_WITNESS_V0_KEYHASH:
            vPubKey.push_back(CPubKey(txIn.scriptWitness.stack.back().begin(), txIn.scriptWitness.stack.back().end()));
            break;
        case TX_SCRIPTHASH:
        {
            std::vector<std::vector<unsigned char>> stack;
            if (!EvalScript(stack, txIn.scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker(), SigVersion::BASE)) {
                return false;
            }
            CScript redeemScript;
            redeemScript = CScript(stack.back().begin(), stack.back().end());
            if (!GetPubKeyFromScript(redeemScript, txIn, vPubKey, depth + 1)) {
                return false;
            }
        }
        break;
        case TX_WITNESS_V0_SCRIPTHASH:
        {
            std::vector<std::vector<unsigned char>> stack;
            stack = txIn.scriptWitness.stack;
            CScript redeemScript;
            redeemScript = CScript(stack.back().begin(), stack.back().end());
            if (!GetPubKeyFromScript(redeemScript, txIn, vPubKey, depth + 1)) {
                return false;
            }
        }
        break;
        default:
            return false;
    }
    return true;
}

bool GetPubKeysFromCoinStakeTx(const CTransactionRef& txCoinStake, std::vector<CPubKey>& vPubKeys)
{
    if (!GetPubKeyFromScript(txCoinStake->vout[0].scriptPubKey, txCoinStake->vin[0], vPubKeys)) {
        return false;
    }

    for (const CPubKey& v_pub_key : vPubKeys) {
        if (!v_pub_key.IsValid())
            return false;
    }

    return true;
}

bool MakeBlockHashExcludedSignature(const CBlock& block, uint256& hashBlock, std::vector<unsigned char>& sig)
{
    const CScript& scriptSig = block.vtx[0]->vin[0].scriptSig;

    auto itr = scriptSig.begin();
    opcodetype op;
    // get last element
    while (GetScriptOp(itr, scriptSig.end(), op, &sig)) {
        if (itr == scriptSig.end()) {
            break;
        }
    }

    if (op >= OP_PUSHDATA1) {
        return error("MakeBlockHashExcludedSignature(): the last element of scriptSig is not signature");
    }

    CMutableTransaction txCoinBase(*block.vtx[0]);
    txCoinBase.vin[0].scriptSig = CScript(scriptSig.begin(), scriptSig.end() - (op + 1));
    CBlock cpBlock = block;
    cpBlock.vtx[0] = MakeTransactionRef(std::move(txCoinBase));
    cpBlock.hashMerkleRoot = BlockMerkleRoot(cpBlock);

    hashBlock = cpBlock.GetBlockHeader().GetHash();

    return true;
}

bool CheckBlockSignature(const CBlock& block, const Consensus::Params& consensusParams)
{
    std::vector<CPubKey> pubkeys;
    if (!GetPubKeysFromCoinStakeTx(block.vtx[1], pubkeys)) {
        return error("CheckBlockSignature(): could not get the public key");
    }
    uint256 hashBlock;
    std::vector<unsigned char> signature;
    if (!MakeBlockHashExcludedSignature(block, hashBlock, signature)) {
        return error("CheckBlockSignature(): could not get the signature and hashblock");
    }
    for (const CPubKey& pubkey : pubkeys) {
        if (pubkey.Verify(hashBlock, signature))
            return true;
    }
    return error("CheckBlockSignature(): Verify Failed signature = %s, hashblock = %s", HexStr(signature), HexStr(hashBlock));
}

} // namespace pos
