#pragma once


// determine how much of an asset x% is = to
int64_t fusion::calculate_asset_share(const int64_t& quantity, const uint64_t& percentage) {
	//percentage uses a scaling factor of 1e6
	//0.01% = 10,000
	//0.1% = 100,000
	//1% = 1,000,000
	//10% = 10,000,000
	//100% = 100,000,000

	//formula is ( quantity * percentage ) / ( 100 * SCALE_FACTOR_1E6 )
	if (quantity <= 0) return 0;

	return mulDiv( uint64_t(quantity), percentage, SCALE_FACTOR_1E8 );
}

/** internal_get_swax_allocations
 *  used during distributions to determine how much of the rewards
 *  go to autocompounding, and to claimable wax for swax holders
 *  amount: the total pool to calculate a share of
 *  swax_divisor: the amount of sWAX or liquified sWAX getting a share of this pool
 *  swax_supply: the combined sWAX and liquified sWAX supply, so we can calculate the quantity to
 *  	give to the swax_divisor
 */

int64_t fusion::internal_get_swax_allocations( const int64_t& amount, const int64_t& swax_divisor, const int64_t& swax_supply ) {
	//contract should have already validated quantity before calling this

	if ( swax_divisor <= 0 ) return 0;

	return mulDiv( uint64_t(amount), uint64_t(swax_divisor), uint128_t(swax_supply) );
}

int64_t fusion::internal_get_wax_owed_to_user(const int64_t& user_stake, const int64_t& total_stake, const int64_t& reward_pool) {
	//user_stake, total_stake and reward_pool should have already been verified to be > 0
	//formula is ( user_stake * reward_pool ) / total_stake
	if ( user_stake <= 0 ) return 0;

	return mulDiv( uint64_t(user_stake), uint64_t(reward_pool), uint128_t(total_stake) );
}

/** internal_liquify
 *  takes swax quantity as input
 *  then calculates the lswax output amount and returns it
 */

int64_t fusion::internal_liquify(const int64_t& quantity, state s) {
	//contract should have already validated quantity before calling this

	/** need to account for initial period where the values are still 0
	 *  also if lswax has not compounded yet, the result will be 1:1
	 */

	if ( s.liquified_swax.amount == s.swax_currently_backing_lswax.amount ) {
		return quantity;
	} else {
		return mulDiv( uint64_t(s.liquified_swax.amount), uint64_t(quantity), uint128_t(s.swax_currently_backing_lswax.amount) );
	}
}

int64_t fusion::internal_unliquify(const int64_t& quantity, state s) {
	//contract should have already validated quantity before calling this
	return mulDiv( uint64_t(s.swax_currently_backing_lswax.amount), uint64_t(quantity), uint128_t(s.liquified_swax.amount) );
}