#pragma once

int64_t fusion::cpu_rental_price_from_seconds(const uint64_t& seconds, const int64_t& price_per_day, const int64_t& amount){
  uint128_t daily_cost = safeMulUInt128( (uint128_t) amount, (uint128_t) price_per_day );
  uint128_t expected_amount = safeMulUInt128( (uint128_t) seconds, daily_cost ) / safeMulUInt128( (uint128_t) SECONDS_PER_DAY, SCALE_FACTOR_1E8 );
  return (int64_t) expected_amount;
}

uint128_t fusion::pool_ratio_1e18(const int64_t& wax_amount, const int64_t& lswax_amount){
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