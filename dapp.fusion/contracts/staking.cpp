#pragma once

void fusion::extend_reward(global&g, rewards& r, staker_struct& self_staker) {

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

	g.total_revenue_distributed.amount = safecast::add(g.total_revenue_distributed.amount, amount_to_distribute);
	g.wax_available_for_rentals.amount = safecast::add(g.wax_available_for_rentals.amount, eco_alloc_i64);
	g.revenue_awaiting_distribution.amount = 0;
	g.incentives_bucket.amount = safecast::add( g.incentives_bucket.amount, lswax_amount_to_issue );
	g.swax_currently_backing_lswax.amount = safecast::add( g.swax_currently_backing_lswax.amount, eco_alloc_i64 );
	g.liquified_swax.amount = safecast::add( g.liquified_swax.amount, lswax_amount_to_issue );

	issue_lswax( lswax_amount_to_issue, _self );
	issue_swax( eco_alloc_i64 );

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

	transfer_tokens( POL_CONTRACT, asset(pol_alloc_i64, WAX_SYMBOL), WAX_CONTRACT, std::string("pol allocation from waxfusion distribution") );
}

int64_t fusion::earned(staker_struct& staker, rewards& r) {

	uint128_t amount_to_add = mulDiv128( uint128_t(staker.swax_balance.amount),
	                                     ( r.rewardPerTokenStored - staker.userRewardPerTokenPaid ),
	                                     SCALE_FACTOR_1E16
	                                   );

	return safecast::safe_cast<int64_t>(amount_to_add);
}

std::pair<staker_struct, staker_struct> fusion::get_stakers(const eosio::name& user, const eosio::name& self) {
    auto staker_itr = staker_t.require_find(user.value, ERR_STAKER_NOT_FOUND);
    auto self_staker_itr = staker_t.require_find(self.value, ERR_STAKER_NOT_FOUND);
    staker_struct staker = staker_struct(*staker_itr);
    staker_struct self_staker = staker_struct(*self_staker_itr);
    return std::make_pair(staker, self_staker);
}

void fusion::modify_staker(staker_struct& staker){
	auto itr = staker_t.require_find(staker.wallet.value, ERR_STAKER_NOT_FOUND);
	staker_t.modify(itr, same_payer, [&](auto &_s){
		_s.claimable_wax = staker.claimable_wax;
		_s.swax_balance = staker.swax_balance;
		_s.last_update = staker.last_update;
		_s.userRewardPerTokenPaid = staker.userRewardPerTokenPaid;
	});
}


uint128_t fusion::reward_per_token(rewards& r)
{
	if ( r.totalSupply == 0 ) return 0;
	
	//the only time periodFinish should be less than now
	//is when the extend_reward function is called
	//which calculates pending rewards up to the previous periodFinish, 
	//then sets a new periodFinish with new rewardRate before users call update_reward
	uint64_t time_elapsed = std::min( now(), r.periodFinish ) - r.lastUpdateTime;

	uint128_t a = r.rewardRate;
	uint128_t b = uint128_t(time_elapsed) * SCALE_FACTOR_1E8;
	uint128_t c = r.totalSupply;

	return r.rewardPerTokenStored + mulDiv128( a, b, c );
}

void fusion::update_reward(staker_struct& staker, rewards& r) {
	
	if( r.lastUpdateTime < r.periodFinish ){
		r.rewardPerTokenStored = reward_per_token(r);
	}

	r.lastUpdateTime = now();

	if(staker.swax_balance.amount > 0){

		int64_t pending_rewards = earned(staker, r);
		staker.claimable_wax.amount += pending_rewards;
		r.totalRewardsPaidOut.amount += pending_rewards;
		check( r.totalRewardsPaidOut <= r.rewardPool, "overdrawn reward pool" );

	}

	staker.userRewardPerTokenPaid = r.rewardPerTokenStored;
	staker.last_update = now();
}

void fusion::zero_distribution(global& g, rewards& r) {
	if( r.lastUpdateTime < r.periodFinish ){
		r.rewardPerTokenStored = reward_per_token(r);
		r.lastUpdateTime = r.periodFinish;
	}

	next_farm nf(r);

	r.lastUpdateTime = nf.lastUpdateTime;
	r.periodFinish = nf.periodFinish;
	r.rewardRate = 0;
}