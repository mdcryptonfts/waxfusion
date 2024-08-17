#pragma once

/**
 * Extends the reward period if the existing period has ended
 * 
 * NOTE: If there are no rewards to distribute, a 
 * `zero_distribution` will occur
 * 
 * @param g - global singleton
 * @param r - rewards singleton
 * @param self_staker - staker_struct that stakes the sWAX backing lsWAX
 */

void fusion::extend_reward(global& g, rewards& r, staker_struct& self_staker) {

    if ( now() <= r.periodFinish ) return;

    if ( g.revenue_awaiting_distribution.amount == 0 ) {
        zero_distribution( g, r );
        return;
    }

    int64_t amount_to_distribute    = max_reward(g, r);
    int64_t user_alloc_i64          = calculate_asset_share( amount_to_distribute, g.user_share_1e6 );
    int64_t pol_alloc_i64           = calculate_asset_share( amount_to_distribute, g.pol_share_1e6 );
    int64_t eco_alloc_i64           = calculate_asset_share( amount_to_distribute, g.ecosystem_share_1e6 );
    int64_t lswax_amount_to_issue   = calculate_lswax_output( eco_alloc_i64, g );

    // If rounding resulted in any leftover waxtoshis, add them to the reward farm
    const int64_t sum           = user_alloc_i64 + pol_alloc_i64 + eco_alloc_i64;
    const int64_t difference    = safecast::sub( amount_to_distribute, sum );

    if ( difference > 0 ) user_alloc_i64 += difference;

    validate_allocations( amount_to_distribute, {user_alloc_i64, pol_alloc_i64, eco_alloc_i64} );

    g.total_revenue_distributed.amount      +=  amount_to_distribute;
    g.wax_available_for_rentals.amount      +=  eco_alloc_i64;
    g.revenue_awaiting_distribution.amount  -=  amount_to_distribute;
    g.incentives_bucket.amount              +=  lswax_amount_to_issue;
    g.swax_currently_backing_lswax.amount   +=  eco_alloc_i64;
    g.liquified_swax.amount                 +=  lswax_amount_to_issue;

    issue_lswax( lswax_amount_to_issue, _self );
    issue_swax( eco_alloc_i64 );

    if( r.lastUpdateTime < r.periodFinish ){
        r.rewardPerTokenStored  = reward_per_token(r);
        r.lastUpdateTime        = r.periodFinish;
    }

    next_farm nf(r);

    r.lastUpdateTime    =   nf.lastUpdateTime;
    r.periodFinish      =   nf.periodFinish;
    r.rewardRate        =   mulDiv128( uint128_t(user_alloc_i64), SCALE_FACTOR_1E8, uint128_t(STAKING_FARM_DURATION) );
    r.rewardPool        +=  asset(user_alloc_i64, WAX_SYMBOL);

    update_reward(self_staker, r);

    r.totalSupply               += uint128_t(eco_alloc_i64);
    self_staker.swax_balance    += asset(eco_alloc_i64, SWAX_SYMBOL);

    transfer_tokens( POL_CONTRACT, asset(pol_alloc_i64, WAX_SYMBOL), WAX_CONTRACT, std::string("pol allocation from waxfusion distribution") );
}

/**
 * Calculates how much staking rewards a `staker` has earned since their `last_update`
 * 
 * @param staker - `staker_struct` containing the user's data
 * @param r - `rewards` singleton with the current farm data
 * 
 * @return int64_t - amount of WAX earned by the staker
 */

int64_t fusion::earned(staker_struct& staker, rewards& r) {

    uint128_t amount_to_add = mulDiv128( uint128_t(staker.swax_balance.amount),
                                         ( r.rewardPerTokenStored - staker.userRewardPerTokenPaid ),
                                         SCALE_FACTOR_1E16
                                       );

    return safecast::safe_cast<int64_t>(amount_to_add);
}

/**
 * Fetches data for a staker, and `self_staker`
 * 
 * @param user - the wallet address of the staker
 * 
 * @return `pair` - staker_struct for the user, and self_staker
 */

std::pair<staker_struct, staker_struct> fusion::get_stakers(const name& user) {
    auto            staker_itr      = staker_t.require_find(user.value, ERR_STAKER_NOT_FOUND);
    auto            self_staker_itr = staker_t.require_find(_self.value, ERR_STAKER_NOT_FOUND);
    staker_struct   staker          = staker_struct(*staker_itr);
    staker_struct   self_staker     = staker_struct(*self_staker_itr);
    return std::make_pair(staker, self_staker);
}

/**
 * Calculates the maximum reward to distribute during `extend_reward`
 * 
 * NOTE: Please see the following Github issue for more details
 * on why this function exists:
 * https://github.com/mdcryptonfts/waxfusion/issues/1
 * 
 * Also, the max_apr in `global2` is the amount that we want sWAX/lsWAX holders 
 * to receive. For this, we need to account for the fact that they are only getting 
 * 85% of rewards (depending on the global state). This is the reason that mulDiv 
 * is called when calculating `adjusted_max_apr`.
 * 
 * @param g - the `global` singleton
 * @param r - the `rewards` singleton
 * 
 * @return `int64_t` - the amount of WAX to distribute
 */

int64_t fusion::max_reward(global& g, rewards& r){
    global2     g2                  = global_s_2.get_or_create( _self, global2{} );
    uint64_t    adjusted_max_apr    = mulDiv( g2.max_staker_apr_1e6, uint64_t(SCALE_FACTOR_1E8), uint128_t(g.user_share_1e6) );
    int64_t     max_yearly_reward   = calculate_asset_share( r.totalSupply, adjusted_max_apr );
    int64_t     max_daily_reward    = safecast::div( max_yearly_reward, int64_t(365) );

    return std::min( max_daily_reward, g.revenue_awaiting_distribution.amount );
}

/**
 * Modifies an existing table row for a `staker`
 * 
 * @param staker - `staker_struct` containing the new data for the user
 */

void fusion::modify_staker(staker_struct& staker){
    auto itr = staker_t.require_find(staker.wallet.value, ERR_STAKER_NOT_FOUND);
    staker_t.modify(itr, same_payer, [&](auto &_s){
        _s.claimable_wax            = staker.claimable_wax;
        _s.swax_balance             = staker.swax_balance;
        _s.last_update              = staker.last_update;
        _s.userRewardPerTokenPaid   = staker.userRewardPerTokenPaid;
    });
}

/**
 * Calculates the current accumulated rewards per token
 * 
 * @param r - `rewards` singleton with current reward pool state
 * 
 * @return `uint128_t` - updated rewardPerTokenStored for the reward pool
 */

uint128_t fusion::reward_per_token(rewards& r)
{
    if ( r.totalSupply == 0 ) return 0;
    
    uint64_t    time_elapsed    = std::min( now(), r.periodFinish ) - std::max( r.periodStart, r.lastUpdateTime ) ;
    uint128_t   a               = r.rewardRate;
    uint128_t   b               = uint128_t(time_elapsed) * SCALE_FACTOR_1E8;
    uint128_t   c               = r.totalSupply;

    return r.rewardPerTokenStored + mulDiv128( a, b, c );
}

/**
 * Updates the current rewards for a `staker`
 * 
 * NOTE: This also updates the reward pool itself (if needed)
 * before updating the `staker`, as not doing so would result
 * in miscalculation of the user's rewards (and potentially
 * overallocation of the reward pool)
 * 
 * Throws if totalRewardsPaidOut > rewardPool (sanity check, should never happen)
 * 
 * @param staker - `staker_struct` containing the user's data
 * @param r - `rewards` singleton with the reward pool state
 */

void fusion::update_reward(staker_struct& staker, rewards& r) {
        
    if( r.lastUpdateTime < r.periodFinish && now() > r.periodStart ){
        r.rewardPerTokenStored = reward_per_token(r);
    }

    r.lastUpdateTime = now();

    if( staker.swax_balance.amount > 0 && now() > r.periodStart ){

        int64_t pending_rewards         =   earned(staker, r);
        staker.claimable_wax.amount     +=  pending_rewards;
        r.totalRewardsPaidOut.amount    +=  pending_rewards;

        check( r.totalRewardsPaidOut <= r.rewardPool, "overdrawn reward pool" );

    }

    staker.userRewardPerTokenPaid   = r.rewardPerTokenStored;
    staker.last_update              = now();
}

/**
 * If there are no rewards to distribute, extends the farm with 0 as `rewardRate`
 * 
 * @param g - `global` singleton
 * @param r - `rewards` singleton
 */

void fusion::zero_distribution(global& g, rewards& r) {
    if( r.lastUpdateTime < r.periodFinish ){
        r.rewardPerTokenStored  = reward_per_token(r);
        r.lastUpdateTime        = r.periodFinish;
    }

    next_farm nf(r);

    r.lastUpdateTime    = nf.lastUpdateTime;
    r.periodFinish      = nf.periodFinish;
    r.rewardRate        = 0;
}