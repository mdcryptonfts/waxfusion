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

	return safecast::safe_cast<int64_t>(result_128);
}

void polcontract::calculate_liquidity_allocations(const liquidity_struct& lp_details, 
																					int64_t& liquidity_allocation,
																					int64_t& wax_bucket_allocation,
																					int64_t& buy_lswax_allocation)
{

    uint128_t liquidity_allocation_scaled = safeMulUInt128( uint128_t(liquidity_allocation), SCALE_FACTOR_1E8 );
    uint128_t sum_of_alcor_and_real_price = safeAddUInt128( uint128_t(lp_details.alcors_lswax_price), uint128_t(lp_details.real_lswax_price) );
    check( liquidity_allocation_scaled > sum_of_alcor_and_real_price, "error calculating liquidity allocation" );
    uint128_t intermediate_result = safeDivUInt128( liquidity_allocation_scaled, sum_of_alcor_and_real_price );
    uint128_t adjusted_intermediate_result = safeMulUInt128( intermediate_result, lp_details.real_lswax_price );
    uint128_t lswax_alloc_128 = safeDivUInt128( adjusted_intermediate_result, SCALE_FACTOR_1E8 );
    buy_lswax_allocation = safecast::safe_cast<int64_t>(lswax_alloc_128);
    wax_bucket_allocation = safeSubInt64( liquidity_allocation, buy_lswax_allocation );

    return;
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

		return safecast::safe_cast<int64_t>(result_128);
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

  	return safecast::safe_cast<int64_t>(result_128);
} 

//convert the sqrtPriceX64 from alcor into actual asset prices for tokenA and tokenB
std::vector<int64_t> polcontract::sqrt64_to_price(const uint128_t& sqrtPriceX64){

	//scale by 1e4 since multiplying together will result in 1e8 scaling
    uint128_t sqrtPrice = safeMulUInt128(sqrtPriceX64, SCALE_FACTOR_1E4) >> 64;

    //now scaled by 1e8 after squaring
    uint128_t price = safeMulUInt128(sqrtPrice, sqrtPrice);

    uint128_t P_tokenA_128 = price;

    //divisor is 1e16 so the result will remain scaled by 1e8 to match lsWAX/WAX decimals
    uint128_t P_tokenB_128 = safeDivUInt128(SCALE_FACTOR_1E16, P_tokenA_128);

	return { safecast::safe_cast<int64_t>(P_tokenA_128), safecast::safe_cast<int64_t>(P_tokenB_128) };
}

//get the price of tokenA A in terms of tokenB based on pool quantities
int64_t polcontract::token_price(const int64_t& amount_A, const int64_t& amount_B){
	//if the 2 inputs are equal, it's a 1:1 ratio
	if( amount_A == amount_B ){
		return SCALE_FACTOR_1E8;
	}	

	//scale the wax_amount before division
	uint128_t amount_A_scaled = safeMulUInt128( uint128_t(amount_A), SCALE_FACTOR_1E8 );

	//make sure division is safe
	check( amount_A_scaled > uint128_t(amount_B), "pool ratio division isnt safe" );
	uint128_t result_128 = safeDivUInt128( amount_A_scaled, uint128_t(amount_B) );

	return safecast::safe_cast<int64_t>(result_128);
}