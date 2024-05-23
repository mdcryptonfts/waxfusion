#pragma once

// determine how much of an asset x% is = to
int64_t polcontract::calculate_asset_share(const int64_t& quantity, const uint64_t& percentage){
	//percentage uses a scaling factor of 1e6
	//0.01% = 10,000
	//0.1% = 100,000
	//1% = 1,000,000
	//10% = 10,000,000
	//100% = 100,000,000

	//formula is ( quantity * percentage ) / ( 100 * SCALE_FACTOR_1E6 )
	if(quantity <= 0) return 0;

	uint128_t divisor = safeMulUInt128( uint128_t(quantity), uint128_t(percentage) );
	
	//since 100 * 1e6 = 1e8, we will just / SCALE_FACTOR_1E8 here to avoid extra unnecessary math
	uint128_t result_128 = safeDivUInt128( divisor, SCALE_FACTOR_1E8 );

  return int64_t(result_128);
}

void polcontract::calculate_liquidity_allocations(const liquidity_struct& lp_details, 
																					int64_t& liquidity_allocation,
																					int64_t& wax_bucket_allocation,
																					int64_t& buy_lswax_allocation)
{
    //since will will be doing some large number math, we will get the max scaling factor for the liquidity allocation
    //and scale it as large as we safely can
    uint128_t scale_factor = max_scale_without_room( uint128_t(liquidity_allocation) );

    //scale the liquidity allocation before doing integer math
    uint128_t liquidity_allocation_scaled = safeMulUInt128( uint128_t(liquidity_allocation), scale_factor );

    //Add the 2 prices together so we can determine the average of the 2 prices
    //we will use this to perfectly balance the weights so 100% of the liquidity allocation
    //gets paired together on alcor. we need to account for alcors price,
    //but also account for the cost of buying lswax from the dapp contract
    uint128_t sum_of_alcor_and_real_price = safeAddUInt128( lp_details.alcors_lswax_price, lp_details.real_lswax_price );

    //This scaled sum is already pretty large, so dividing first is safer than more multiplication
    //Divide liquidity_allocation_scaled by the sum of alcors_lswax_price and real_lswax_price
    //Since the liquidity allocation is scaled so large, there should be 0 chance of it ever
    //being smaller than sum_of_alcor_and_real_price, however we should check anyway just to ensure safety
    check( liquidity_allocation_scaled > sum_of_alcor_and_real_price, "error calculating liquidity allocation" );
    uint128_t intermediate_result = safeDivUInt128( liquidity_allocation_scaled, sum_of_alcor_and_real_price );

    //Multiply intermediate_result by real_lswax_price to determine how much wax to spend, then scale down by scale_factor
    uint128_t adjusted_intermediate_result = safeMulUInt128( intermediate_result, lp_details.real_lswax_price );
    uint128_t lswax_alloc_128 = safeDivUInt128( adjusted_intermediate_result, scale_factor );

    //set the buckets created above to their appropriate amounts
    buy_lswax_allocation = int64_t(lswax_alloc_128);
    wax_bucket_allocation = safeSubInt64( liquidity_allocation, buy_lswax_allocation );

    return;
}


// determine how much of an e18 amount x% is = to
// this function returns a scaled amount (1e18) so it still needs to be reduced later
uint128_t polcontract::calculate_share_from_e18(const uint128_t& amount, const uint64_t& percentage){
	//percentage uses a scaling factor of 1e6
	//0.01% = 10,000
	//0.1% = 100,000
	//1% = 1,000,000
	//10% = 10,000,000
	//100% = 100,000,000

	//formula is ( amount * percentage ) / ( 100 * SCALE_FACTOR_1E6 )
	if(amount == 0) return 0;

	/** larger amounts might cause overflow when scaling too large, so we will scale
	 *  according to the size of the amount
	 */

	//get a safe scaling factor to multiply the amount
	uint128_t scale_factor = max_scale_with_room( amount );
	uint128_t amount_scaled = safeMulUInt128( scale_factor, amount );

	//multiply by the percentage before dividing, to avoid loss of precision
	uint128_t percentage_share_scaled = safeMulUInt128( amount_scaled, uint128_t(percentage) );

	//since the percentage passed to this function is scaled by 1e6
	//and the formula for calculating a percentage is num * percentage / 100
	//we need to divide by 1e6 * 100 which is 1e8
	//and also account for the scale_factor to shrink the number back down
	uint128_t dividend = safeMulUInt128( scale_factor, SCALE_FACTOR_1E8 );

	//make sure division is safe
	check( percentage_share_scaled > dividend, "cs_e18 inputs out of bounds" );

	uint128_t percentage_share = safeDivUInt128( percentage_share_scaled, dividend );

  	return uint128_t(percentage_share);
}

int64_t polcontract::internal_liquify(const int64_t& quantity, dapp_tables::state s){
	//contract should have already validated quantity before calling this
	//always make sure to pass a positive number as the quantity

    /** need to account for initial period where the values are still 0
     *  also if lswax has not compounded yet, the result will be 1:1
     */
	
    if( (s.liquified_swax.amount == 0 && s.swax_currently_backing_lswax.amount == 0)
    	||
    	(s.liquified_swax.amount == s.swax_currently_backing_lswax.amount)
     ){
      return quantity;
    } else {
		//multiply quantity by liquified swax amount before division
		uint128_t quantity_scaled_by_lswax = safeMulUInt128( uint128_t(s.liquified_swax.amount), uint128_t(quantity) );

		//make sure division is safe
		check( quantity_scaled_by_lswax > uint128_t(s.swax_currently_backing_lswax.amount), "liquify inputs out of bounds" );

		//divide by the amount of swax that's currently backing lswax
		uint128_t result_128 = safeDivUInt128( quantity_scaled_by_lswax, uint128_t(s.swax_currently_backing_lswax.amount) );

		return int64_t(result_128);      	
    }		
}

int64_t polcontract::internal_unliquify(const int64_t& quantity, dapp_tables::state s){
	//contract should have already validated quantity before calling this
	//always make sure to pass a positive number as the quantity

	//multiply quantity by amount of swax backing lswax before division
	uint128_t quantity_scaled_by_swax = safeMulUInt128( uint128_t(s.swax_currently_backing_lswax.amount), uint128_t(quantity) );
  	
	//double check that division is safe
	check( quantity_scaled_by_swax > uint128_t(s.liquified_swax.amount), "unliquify inputs out of bounds" );

	//divide by the amount of lswax in existence
  	uint128_t result_128 = safeDivUInt128( quantity_scaled_by_swax, uint128_t(s.liquified_swax.amount) );

  	return int64_t(result_128);  
} 

/** pool_ratio_1e18
 *  returns a scaled ratio of the wax/lswax pool on alcor
 *  needs to be scaled back down later before sending/storing assets
 * 	if this wasn't scaled, wax_amount < lswax_amount would result in 0
 */

uint128_t polcontract::pool_ratio_1e18(const int64_t& wax_amount, const int64_t& lswax_amount){
	//if the 2 inputs are equal, it's a 1:1 ratio
	if( wax_amount == lswax_amount ){
		return SCALE_FACTOR_1E18;
	}	

	//scale the wax_amount before division
	uint128_t wax_amount_1e18 = safeMulUInt128( uint128_t(wax_amount), SCALE_FACTOR_1E18 );

	//make sure division is safe
	check( wax_amount_1e18 > uint128_t(lswax_amount), "pool ratio division isnt safe" );

	return safeDivUInt128( wax_amount_1e18, uint128_t(lswax_amount) );
}