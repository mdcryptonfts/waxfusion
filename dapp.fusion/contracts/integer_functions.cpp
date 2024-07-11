#pragma once


/**
 * Calculates how much of an asset belongs to a certain percentage share
 * 
 * @param quantity - `int64_t` asset amount to calculate a share of
 * @param percentage - `uint64_t` the percentage of `quantity` to allocate, scaled by 1e6
 * 
 * @return int64_t - the calculated output amount
 */

int64_t fusion::calculate_asset_share(const int64_t& quantity, const uint64_t& percentage) {
    // Percentage uses a scaling factor of 1e6
    // 0.01% = 10,000
    // 0.1% = 100,000
    // 1% = 1,000,000
    // 10% = 10,000,000
    // 100% = 100,000,000

    if (quantity <= 0) return 0;

    return mulDiv( uint64_t(quantity), percentage, SCALE_FACTOR_1E8 );
}

/**
 * Converts an sWAX amount into its lsWAX value
 * 
 * @param quantity - `int64_t` amount of sWAX to convert
 * @param g - global singleton
 * 
 * @return int64_t - calculated output amount of lsWAX
 */

int64_t fusion::calculate_lswax_output(const int64_t& quantity, global& g) {

    if ( g.liquified_swax.amount == g.swax_currently_backing_lswax.amount ) {
        return quantity;
    } else {
        return mulDiv( uint64_t(g.liquified_swax.amount), uint64_t(quantity), uint128_t(g.swax_currently_backing_lswax.amount) );
    }

}

/**
 * Converts an lsWAX amount into its underlying sWAX value
 * 
 * @param quantity - `int64_t` amount of lsWAX to convert
 * @param g - global singleton
 * 
 * @return int64_t - calculated output amount of sWAX
 */

int64_t fusion::calculate_swax_output(const int64_t& quantity, global& g) {
    return mulDiv( uint64_t(g.swax_currently_backing_lswax.amount), uint64_t(quantity), uint128_t(g.liquified_swax.amount) );
}
