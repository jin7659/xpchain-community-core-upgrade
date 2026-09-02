// Copyright (c) 2018-2026 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/reward.h>
#include <pos/height.h>
#include <streams.h>
#include <hash.h>

#include <algorithm>

namespace pos {

double_t GetAnnualRate(int nHeight, const Consensus::Params& consensusParams)
{
    int nSubsidyReducingInterval = 60 * 24 * 365;
    if(!IsPoSHeight(nHeight, consensusParams))
    {
        return 0;
    }
    else if(IsPoSHeight(nHeight, consensusParams) && nHeight <= nSubsidyReducingInterval)
    {
        return 0.10;
    }
    else if(nSubsidyReducingInterval < nHeight && nHeight <= nSubsidyReducingInterval * 2)
    {
        return 0.09;
    }
    else if(nSubsidyReducingInterval * 2 < nHeight && nHeight <= nSubsidyReducingInterval * 3)
    {
        return 0.08;
    }
    else if(nSubsidyReducingInterval * 3 < nHeight && nHeight <= nSubsidyReducingInterval * 4)
    {
        return 0.07;
    }
    else if(nSubsidyReducingInterval * 4 < nHeight && nHeight <= nSubsidyReducingInterval * 5)
    {
        return 0.06;
    }
    else if(nSubsidyReducingInterval * 5 < nHeight)
    {
        return 0.05;
    }

    // Defensive fallback for compiler return-path analysis.
    return 0.05;
}

CAmount GetProofOfStakeReward(int nHeight, CAmount nAmount, uint32_t nTime, const Consensus::Params& consensusParams)
{
    if(!IsPoSHeight(nHeight, consensusParams))
    {
        return 0;
    }

    double_t dRewardCurveMaximum = 1.02500000;
    double_t dRewardCurveLimit = 1.00000000;
    double_t dRewardCurveBase = 0.01800000;
    double_t dRewardCurveSteepness = 0.00000285;

    if(nTime < consensusParams.nStakeMinAge)
    {
        return 0;
    }

    nTime = std::min(nTime, (uint32_t)consensusParams.nStakeMaxAge);

    CAmount annual = nAmount * GetAnnualRate(nHeight, consensusParams);

    double_t coefficient = dRewardCurveMaximum / (1.0 + (dRewardCurveMaximum / dRewardCurveBase - 1.0) * exp(-dRewardCurveSteepness * nTime));
    coefficient = std::min(coefficient, dRewardCurveLimit);

    return (CAmount) (annual * coefficient * nTime / (365 * 24 * 60 * 60));
}

uint256 GetRewardHash(const std::vector<std::pair<CScript, CAmount>>& vReward, CTransactionRef txCoinStake, uint32_t nTime)
{
    CDataStream ss(SER_GETHASH, 0);
    for(const std::pair<CScript, CAmount>& p : vReward)
    {
        ss << p.first << p.second;
    }
    ss << nTime << txCoinStake->vin[0];
    return Hash(ss.begin(), ss.end());
}

} // namespace pos
