#pragma once

/** 
 * extend_reward function, but without modifying any state
 * 
 * @param g - global singleton
 * @param r - rewards singleton
 * @param self_staker - staker_struct that stores data about sWAX backing lsWAX
 */

void fusion::readonly_extend_reward(global&g, rewards& r, staker_struct& self_staker) {

    if ( now() <= r.periodFinish ) return;

    if ( g.revenue_awaiting_distribution.amount == 0 ) {
        zero_distribution( g, r );
        return;
    }

    int64_t amount_to_distribute = g.revenue_awaiting_distribution.amount;
    int64_t user_alloc_i64 = calculate_asset_share( amount_to_distribute, g.user_share_1e6 );
    int64_t pol_alloc_i64 = calculate_asset_share( amount_to_distribute, g.pol_share_1e6 );
    int64_t eco_alloc_i64 = calculate_asset_share( amount_to_distribute, g.ecosystem_share_1e6 );

    int64_t lswax_amount_to_issue = calculate_lswax_output( eco_alloc_i64, g );

    //if rounding resulted in any leftover waxtoshis, add them to the reward farm
    const int64_t sum = user_alloc_i64 + pol_alloc_i64 + eco_alloc_i64;
    const int64_t difference = safecast::sub( amount_to_distribute, sum );
    if ( difference > 0 ) user_alloc_i64 += difference;

    validate_allocations( amount_to_distribute, {user_alloc_i64, pol_alloc_i64, eco_alloc_i64} );

    if( r.lastUpdateTime < r.periodFinish ){
        r.rewardPerTokenStored = reward_per_token(r);
        r.lastUpdateTime = r.periodFinish;
    }

    next_farm nf(r);

    r.lastUpdateTime = nf.lastUpdateTime;
    r.periodFinish = nf.periodFinish;
    r.rewardRate = mulDiv128( uint128_t(user_alloc_i64), SCALE_FACTOR_1E8, uint128_t(STAKING_FARM_DURATION) );
    r.rewardPool += asset(user_alloc_i64, WAX_SYMBOL);

    update_reward(self_staker, r);
    r.totalSupply += uint128_t(eco_alloc_i64);
    self_staker.swax_balance += asset(eco_alloc_i64, SWAX_SYMBOL);
}

/** 
 * sync_epoch function, but without modifying any state
 * 
 * @param g - global singleton
 */

inline void fusion::readonly_sync_epoch(global& g) {

  uint64_t next_epoch_start_time = g.last_epoch_start_time + g.seconds_between_epochs;

  while ( now() >= next_epoch_start_time ) {

    eosio::name next_cpu_contract = get_next_cpu_contract( g );

    g.last_epoch_start_time = next_epoch_start_time;
    g.current_cpu_contract = next_cpu_contract;

    auto epoch_itr = epochs_t.find(next_epoch_start_time);

    next_epoch_start_time += g.seconds_between_epochs;
  }
}

/**
 * readonly action to show if there is a need to unstake cpu
 * 
 * @param epoch_id - pass `0` for the current epoch. pass a valid epoch_id to check
 * a specific epoch.
 * 
 * @return uint64_t epoch id to unstake from. if error, will return one of the `enum`
 * values for predetermined error codes
 */

[[eosio::action, eosio::read_only]] uint64_t fusion::showexpcpu(const uint64_t& epoch_id)
{

    global g = global_s.get();

    uint64_t epoch_to_check = epoch_id == 0 ? g.last_epoch_start_time - g.seconds_between_epochs : epoch_id;

    auto epoch_itr = epochs_t.find( epoch_to_check );
    if(epoch_itr == epochs_t.end()) return uint64_t(NOT_FOUND);
    if(epoch_itr->time_to_unstake > now()) return uint64_t(NOT_TIME_YET);

    del_bandwidth_table del_tbl( SYSTEM_CONTRACT, epoch_itr->cpu_wallet.value );
    if ( del_tbl.begin() == del_tbl.end() ) {
        return uint64_t(NOTHING_TO_UNSTAKE);
    }

    return epoch_to_check;
}

/**
 * allows front ends to see if there are any refunds to claim from system contract
 * 
 * @return bool, whether there are refunds to claim or not
 */

[[eosio::action, eosio::read_only]] bool fusion::showrefunds()
{
    global g = global_s.get();

    for (name ctrct : g.cpu_contracts) {
        refunds_table refunds_t = refunds_table( SYSTEM_CONTRACT, ctrct.value );

        auto refund_itr = refunds_t.find( ctrct.value );

        if ( refund_itr != refunds_t.end() && refund_itr->request_time + seconds(REFUND_DELAY_SEC) <= current_time_point() ) {
            return true;
        }
    }

    return false;
}

/**
 * allows front ends to view the claimable rewards of a `user`
 * 
 * @param user - the wax address of the user to view rewards for
 * 
 * @return asset containing the claimable wax balance for `user`
 */

[[eosio::action, eosio::read_only]] asset fusion::showreward(const name& user)
{
    global g = global_s.get();
    rewards r = rewards_s.get();

    readonly_sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user, _self);

    readonly_extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);

    asset claimable_wax = staker.claimable_wax;

    return claimable_wax;
}

/**
 * allows front ends to check if there are voting rewards to claim from system contract
 * 
 * @return vector<name> of the `cpu_contracts` to claim from
 */

[[eosio::action, eosio::read_only]] vector<name> fusion::showvoterwds()
{
    global g = global_s.get();
    vector<eosio::name> contracts_with_rewards {};

    for (name ctrct : g.cpu_contracts) {
        voters_ns::voters_table voters_t(SYSTEM_CONTRACT, SYSTEM_CONTRACT.value);
        auto itr = voters_t.find(ctrct.value);
        if( itr != voters_t.end() && 
            uint64_t(itr->last_claim_time.sec_since_epoch()) + days_to_seconds(1) <= now() && 
            itr->unpaid_voteshare > 0.0 )
        {
            contracts_with_rewards.push_back(ctrct);
        }
    }

    return contracts_with_rewards;
}