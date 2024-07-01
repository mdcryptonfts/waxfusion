#pragma once

struct next_farm {
    uint64_t lastUpdateTime;
    uint64_t periodFinish;

    next_farm(const rewards& r)
    {
        uint64_t durations_elapsed = (eosio::current_time_point().sec_since_epoch() - r.periodFinish) / STAKING_FARM_DURATION;
        lastUpdateTime = r.lastUpdateTime + (STAKING_FARM_DURATION * durations_elapsed);
        periodFinish = lastUpdateTime + STAKING_FARM_DURATION;
    }

    next_farm() = default;
};


struct reward_struct {
    uint64_t        periodStart;
    uint64_t        periodFinish;
    uint128_t       rewardRate;
    uint64_t        rewardsDuration;
    uint64_t        lastUpdateTime;
    uint128_t       rewardPerTokenStored;
    eosio::asset    rewardPool;
    eosio::asset    totalRewardsPaidOut;
    uint128_t       totalSupply;

    reward_struct(const rewards& farm)
        : periodStart(farm.periodStart),
          periodFinish(farm.periodFinish),
          rewardRate(farm.rewardRate),
          rewardsDuration(farm.rewardsDuration),
          lastUpdateTime(farm.lastUpdateTime),
          rewardPerTokenStored(farm.rewardPerTokenStored),
          rewardPool(farm.rewardPool),
          totalRewardsPaidOut(farm.totalRewardsPaidOut),
          totalSupply(farm.totalSupply) {}
    
    reward_struct() = default;      
};

struct staker_struct {
    eosio::name     wallet;
    eosio::asset    swax_balance = ZERO_SWAX;
    uint64_t        last_update = eosio::current_time_point().sec_since_epoch();
    eosio::asset    claimable_wax = ZERO_WAX;
    uint128_t       userRewardPerTokenPaid = 0;

    staker_struct(const stakers& staker)
        : wallet(staker.wallet),
          swax_balance(staker.swax_balance),
          last_update(staker.last_update),
          claimable_wax(staker.claimable_wax),
          userRewardPerTokenPaid(staker.userRewardPerTokenPaid) {}
    
    staker_struct() = default;  
};