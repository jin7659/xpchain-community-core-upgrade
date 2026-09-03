// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/staker.h>
#include <pos/height.h>
#include <pos/kernel.h>
#include <pos/reward.h>
#include <pos/stake.h>

#include <chain.h>
#include <chainparams.h>
#include <miner.h>
#include <net.h>
#include <pow.h>
#include <timedata.h>
#include <util.h>
#include <utiltime.h>
#include <validation.h>

namespace pos {

static unsigned int GetnBits(const CBlockIndex* pIndexLast, const Consensus::Params& params)
{
    assert(pIndexLast);
    assert(pIndexLast->pprev);
    assert(pIndexLast->nHeight + 1 > params.nSwitchHeight);
    return CalculateNextWorkRequired(pIndexLast, pIndexLast->pprev->GetBlockTime(), params);
}

static bool GetPrevBlockIndex(const uint256& hashBlock, CBlockIndex** pIndex)
{
    LOCK(cs_main);
    *pIndex = LookupBlockIndex(hashBlock);
    if (*pIndex == nullptr) {
        return false;
    }
    return true;
}

static void XPChainMinter(const std::shared_ptr<IStakeableWallet>& wallet)
{
    LogPrintf("CPUMiner started for proof-of-stake\n");
    RenameThread("xpchain-stake-minter");

    try
    {
        while (true)
        {
            int64_t start = GetTimeMillis();

            if (wallet->IsLocked())
            {
                MilliSleep(1000);
                continue;
            }

            if (IsInitialBlockDownload()) {
                MilliSleep(1000);
                continue;
            }

            if (!IsPoSHeight(chainActive.Height() + 1, Params().GetConsensus()))
            {
                MilliSleep(1000);
                continue;
            }

            CBlockIndex* pIndexLast = chainActive.Tip();
            assert(pIndexLast);

            unsigned int nBits = GetnBits(pIndexLast, Params().GetConsensus());
            uint32_t nTime = std::max(GetAdjustedTime(), pIndexLast->GetMedianTimePast() + 1);

            std::vector<StakeCandidate> vCandidates;
            wallet->GetStakeCandidates(vCandidates);

            for (const StakeCandidate& candidate : vCandidates) {
                CBlockIndex* pprevIndex;
                if (!GetPrevBlockIndex(candidate.hashBlock, &pprevIndex)) {
                    continue;
                }
                uint256 hashProofOfStake;
                CBlock prevblock;
                assert(pprevIndex);
                if (!ReadBlockFromDisk(prevblock, pprevIndex, Params().GetConsensus())) {
                    continue;
                }
                if (CheckStakeKernelHash(nBits, pprevIndex->GetBlockTime(), GetSizeOfCompactSize(prevblock.vtx.size()) + sizeof(CBlockHeader), candidate.txout.nValue, candidate.outpoint.n, nTime, hashProofOfStake))
                {
                    CScript scriptDummy;
                    CAmount nFees;
                    CTransactionRef txCoinStake;
                    txnouttype t;
                    std::vector<std::vector<unsigned char>> a;
                    Solver(candidate.txout.scriptPubKey, t, a);
                    if (!wallet->CreateCoinStake(candidate, txCoinStake, nFees))
                    {
                        continue;
                    }
                    std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(scriptDummy, wallet.get(), nTime, nBits, txCoinStake, nFees, pIndexLast));
                    if (!pblocktemplate.get())
                    {
                        continue;
                    }
                    else
                    {
                        CBlock* pblock = &pblocktemplate->block;
                        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
                        if (!ProcessNewBlock(Params(), shared_pblock, true, nullptr))
                        {
                            continue;
                        }
                        LogPrintf("success! hash = %s\n", pblock->GetHash().ToString().c_str());
                        break;
                    }
                }
            }
            int64_t end = GetTimeMillis();
            MilliSleep(std::max(0ll, 1000ll - (end - start)));
        }
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("XPChainMiner terminated\n");
        return;
    }
}

static void ThreadStakeMinter(const std::shared_ptr<IStakeableWallet>& wallet)
{
    LogPrintf("ThreadStakeMinter started %s\n", wallet->GetWalletName());
    if (!gArgs.GetBoolArg("-minting", true))
    {
        LogPrintf("nominting\n");
        return;
    }

    while (true) {
        try
        {
            XPChainMinter(wallet);
            LogPrintf("ThreadStakeMinter exiting\n");
            return;
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ThreadStakeMinter()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ThreadStakeMinter()");
        }
        LogPrintf("ThreadStakeMinter restarting...\n");
        MilliSleep(1000);
    }
}

void MintStake(boost::thread_group& threadGroup, const std::shared_ptr<IStakeableWallet>& wallet)
{
    threadGroup.create_thread(boost::bind(&ThreadStakeMinter, wallet));
}

} // namespace pos
