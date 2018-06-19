// Copyright (c) 2018 The Merit Foundation developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pog2/reward.h"

#include <algorithm>
#include <numeric>

namespace pog2
{
    namespace
    {
        const CAmount FIXED_PRECISION = 1000;
        const int INVITES_PER_WINNER = 1;

    }

    CAmount TotalCgs(const Entrants& winners)
    {
        return std::accumulate(std::begin(winners), std::end(winners), CAmount{0},
                [](CAmount acc, const Entrant& e)
                {
                    return acc + e.cgs;
                });
    }

    CAmount ProportionalRewards(pog::Rewards& rewards, CAmount total_reward, const Entrants& winners) {
        auto total_cgs = TotalCgs(winners);

        rewards.resize(winners.size());
        std::transform(std::begin(winners), std::end(winners), std::back_inserter(rewards),
                [total_reward, total_cgs](const Entrant& v)
                {
                    double percent = (v.cgs*FIXED_PRECISION) / total_cgs;
                    CAmount reward = (total_reward * percent) / FIXED_PRECISION;
                    assert(reward <= total_reward);
                    return pog::AmbassadorReward{v.address_type, v.address, reward};
                });

        pog::Rewards filtered_rewards;
        filtered_rewards.reserve(rewards.size());
        std::copy_if(std::begin(rewards), std::end(rewards),
                std::back_inserter(filtered_rewards),
                [](const pog::AmbassadorReward& reward) {
                    return reward.amount > 0;
                });

        return 
            std::accumulate(std::begin(filtered_rewards), std::end(filtered_rewards), CAmount{0},
                    [](CAmount acc, const pog::AmbassadorReward& reward)
                    {
                        return acc + reward.amount;
                    });
    }


    pog::AmbassadorLottery RewardAmbassadors(
            int height,
            const Entrants& old_winners,
            const Entrants& new_winners,
            CAmount total_reward)
    {
        auto total_new_reward = std::floor(total_reward * 0.20);
        auto total_old_reward = total_reward - total_new_reward;

        assert(total_old_reward > total_new_reward);

        pog::Rewards old_rewards;
        auto total_old_rewarded = ProportionalRewards(old_rewards, total_old_reward, old_winners);

        pog::Rewards new_rewards;
        auto total_new_rewarded = ProportionalRewards(new_rewards, total_new_reward, new_winners);

        assert(total_old_rewarded >= 0);
        assert(total_old_rewarded <= total_reward);
        assert(total_new_rewarded >= 0);
        assert(total_new_rewarded <= total_reward - total_old_rewarded);

        auto remainder = total_reward - total_old_rewarded - total_new_rewarded;

        assert(remainder >= 0);
        assert(remainder <= total_reward);

        pog::Rewards rewards;
        rewards.reserve(old_rewards.size() + new_rewards.size());
        rewards.insert(rewards.end(), old_rewards.begin(), old_rewards.end());
        rewards.insert(rewards.end(), new_rewards.begin(), new_rewards.end());

        return {rewards, remainder};
    }

    int ComputeTotalInviteLotteryWinners(
            const pog::InviteLotteryParamsVec& lottery_points,
            const Consensus::Params& params)
    {
        assert(lottery_points.size() == 2);

        const auto& block1 = lottery_points[0];
        const auto& block2 = lottery_points[1];

        LogPrint(BCLog::VALIDATION, "Invites used: %d created: %d period: %d used per block: %d\n",
                block1.invites_used,
                block1.invites_created,
                params.daedalus_block_window,
                block1.mean_used);


        int min_total_winners = 0;
        if(block1.invites_created <= (block1.blocks / params.imp_miner_reward_for_every_x_blocks)) {
            min_total_winners = block1.invites_used + 
                (block1.blocks / params.imp_min_one_invite_for_every_x_blocks);
        }

        const double mean_diff = block1.mean_used - block2.mean_used;

        //Assume we need more or less than what was used before.
        //This allows invites to grow or shrink exponentially.
        const int change = mean_diff >= 0 ?
            std::ceil(mean_diff) : 
            std::floor(mean_diff);

        const int total_winners = std::max(
                min_total_winners,
                static_cast<int>(std::floor(block1.mean_used) + change));

        assert(total_winners >= 0);
        return total_winners;
    }

    pog::InviteRewards RewardInvites(const referral::ConfirmedAddresses& winners)
    {
        assert(winners.size() >= 0);

        pog::InviteRewards rewards(winners.size());
        std::transform(winners.begin(), winners.end(), rewards.begin(),
                [](const referral::ConfirmedAddress& winner) {
                    return pog::InviteReward {
                        winner.address_type,
                        winner.address,
                        INVITES_PER_WINNER
                    };
                });

        assert(rewards.size() == winners.size());
        return rewards;
    }

} // namespace pog2