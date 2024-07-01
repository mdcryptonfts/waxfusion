#pragma once


// determine how much of an asset x% is == to
int64_t fusion::calculate_asset_share(const int64_t& quantity, const uint64_t& percentage) {
    //percentage uses a scaling factor of 1e6
    //0.01% = 10,000
    //0.1% = 100,000
    //1% = 1,000,000
    //10% = 10,000,000
    //100% = 100,000,000

    if (quantity <= 0) return 0;

    return mulDiv( uint64_t(quantity), percentage, SCALE_FACTOR_1E8 );
}

/** calculate_lswax_output
 *  takes swax quantity as input
 *  then calculates the lswax output amount and returns it
 */

int64_t fusion::calculate_lswax_output(const int64_t& quantity, global& g) {

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

int64_t fusion::calculate_swax_output(const int64_t& quantity, global& g) {
    return mulDiv( uint64_t(g.swax_currently_backing_lswax.amount), uint64_t(quantity), uint128_t(g.liquified_swax.amount) );
}
