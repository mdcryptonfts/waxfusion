#pragma once

// determine how much of an asset x% is = to
int64_t polcontract::calculate_asset_share(const int64_t& quantity, const uint64_t& percentage) {
	//percentage uses a scaling factor of 1e6
	//0.01% = 10,000
	//0.1% = 100,000
	//1% = 1,000,000
	//10% = 10,000,000
	//100% = 100,000,000

	if (quantity <= 0) return 0;

	return mulDiv( uint64_t(quantity), percentage, SCALE_FACTOR_1E8 );
}

void polcontract::calculate_liquidity_allocations(const liquidity_struct& lp_details,
    int64_t& liquidity_allocation,
    int64_t& wax_bucket_allocation,
    int64_t& buy_lswax_allocation)
{

	uint128_t sum_of_alcor_and_real_price = safecast::add( uint128_t(lp_details.alcors_lswax_price), uint128_t(lp_details.real_lswax_price) );
	uint128_t lswax_alloc_128 = mulDiv128( uint128_t(liquidity_allocation), uint128_t(lp_details.real_lswax_price), sum_of_alcor_and_real_price );

	buy_lswax_allocation = safecast::safe_cast<int64_t>(lswax_alloc_128);
	wax_bucket_allocation = safecast::sub( liquidity_allocation, buy_lswax_allocation );
}

/** calculate_lswax_output
 *  takes swax quantity as input
 *  then calculates the lswax output amount and returns it
 */

int64_t polcontract::calculate_lswax_output(const int64_t& quantity, dapp_tables::global& g) {

	if ( g.liquified_swax.amount == g.swax_currently_backing_lswax.amount ) {
		return quantity;
	} else {
		return mulDiv( uint64_t(g.liquified_swax.amount), uint64_t(quantity), uint128_t(g.swax_currently_backing_lswax.amount) );
	}

}

/** calculate_swax_output
 *  takes lswax quantity as input
 *  then calculates the swax output amount and returns it
 */

int64_t polcontract::calculate_swax_output(const int64_t& quantity, dapp_tables::global& g) {
	return mulDiv( uint64_t(g.swax_currently_backing_lswax.amount), uint64_t(quantity), uint128_t(g.liquified_swax.amount) );
}

//convert the sqrtPriceX64 from alcor into actual asset prices for tokenA and tokenB
std::vector<int64_t> polcontract::sqrt64_to_price(const uint128_t& sqrtPriceX64) {

    uint128_t priceX64 = mulDiv128( sqrtPriceX64, sqrtPriceX64, TWO_POW_64 );
    uint128_t P_tokenA_128 = mulDiv128( priceX64, SCALE_FACTOR_1E8, TWO_POW_64 );
	uint128_t P_tokenB_128 = safecast::div( SCALE_FACTOR_1E16, P_tokenA_128 );
	return { safecast::safe_cast<int64_t>(P_tokenA_128), safecast::safe_cast<int64_t>(P_tokenB_128) };

}

//get the price of tokenA A in terms of tokenB based on pool quantities
//since we are only working with lsWAX and WAX which both have 8 decimals, SCALE_FACTOR_1E8
int64_t polcontract::token_price(const int64_t& amount_A, const int64_t& amount_B) {

	if ( amount_A == amount_B ) return safecast::safe_cast<int64_t>(SCALE_FACTOR_1E8);

	return mulDiv( uint64_t(amount_A), safecast::safe_cast<uint64_t>(SCALE_FACTOR_1E8), uint128_t(amount_B) );
}