#pragma once

void polcontract::add_liquidity( state3& s, liquidity_struct& lp_details ) {

  int64_t max_wax_possible    = mulDiv( uint64_t(s.wax_bucket.amount), safecast::safe_cast<uint64_t>(SCALE_FACTOR_1E8), uint128_t(lp_details.alcors_lswax_price) );
  int64_t max_lswax_possible  = s.lswax_bucket.amount;

  // Determine the limiting factor
  int64_t wax_to_add, lswax_to_add;
  if (max_wax_possible <= max_lswax_possible) {
    // WAX is the limiting factor, or the weights are equal
    wax_to_add    = s.wax_bucket.amount;
    lswax_to_add  = mulDiv( uint64_t(wax_to_add), safecast::safe_cast<uint64_t>(SCALE_FACTOR_1E8), uint128_t(lp_details.alcors_lswax_price) );

  } else {
    // LSWAX is the limiting factor
    lswax_to_add = s.lswax_bucket.amount;
    wax_to_add = mulDiv( uint64_t(lswax_to_add), uint64_t(lp_details.alcors_lswax_price), SCALE_FACTOR_1E8 );
  }

  lp_details.poolA.amountToAdd = lp_details.aIsWax ? asset( wax_to_add, WAX_SYMBOL ) : asset( lswax_to_add, LSWAX_SYMBOL );
  lp_details.poolB.amountToAdd = !lp_details.aIsWax ? asset( wax_to_add, WAX_SYMBOL ) : asset( lswax_to_add, LSWAX_SYMBOL );

  lp_details.poolA.minAsset = lp_details.aIsWax ? asset( calculate_asset_share( wax_to_add, 99500000), WAX_SYMBOL ) : asset( calculate_asset_share( lswax_to_add, 99500000), LSWAX_SYMBOL );
  lp_details.poolB.minAsset = !lp_details.aIsWax ? asset( calculate_asset_share( wax_to_add, 99500000), WAX_SYMBOL ) : asset( calculate_asset_share( lswax_to_add, 99500000), LSWAX_SYMBOL );

  if (lp_details.aIsWax) {

    check( s.wax_bucket.amount >= lp_details.poolA.amountToAdd.amount, "overallocation error" );
    check( s.lswax_bucket.amount >= lp_details.poolB.amountToAdd.amount, "overallocation error" );

    s.wax_bucket.amount   = safecast::sub( s.wax_bucket.amount, lp_details.poolA.amountToAdd.amount );
    s.lswax_bucket.amount = safecast::sub( s.lswax_bucket.amount, lp_details.poolB.amountToAdd.amount );

  } else {

    check( s.wax_bucket.amount >= lp_details.poolB.amountToAdd.amount, "overallocation error" );
    check( s.lswax_bucket.amount >= lp_details.poolA.amountToAdd.amount, "overallocation error" );

    s.wax_bucket.amount   = safecast::sub( s.wax_bucket.amount, lp_details.poolB.amountToAdd.amount );
    s.lswax_bucket.amount = safecast::sub( s.lswax_bucket.amount, lp_details.poolA.amountToAdd.amount );

  }

  deposit_liquidity_to_alcor(lp_details);
}

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

void polcontract::calculate_liquidity_allocations(  const liquidity_struct& lp_details,
                                                    int64_t& liquidity_allocation,
                                                    int64_t& wax_bucket_allocation,
                                                    int64_t& buy_lswax_allocation
                                                    )
{

    uint128_t sum_of_alcor_and_real_price   = safecast::add( uint128_t(lp_details.alcors_lswax_price), uint128_t(lp_details.real_lswax_price) );
    uint128_t lswax_alloc_128               = mulDiv128( uint128_t(liquidity_allocation), uint128_t(lp_details.real_lswax_price), sum_of_alcor_and_real_price );
    buy_lswax_allocation                    = safecast::safe_cast<int64_t>(lswax_alloc_128);
    wax_bucket_allocation                   = safecast::sub( liquidity_allocation, buy_lswax_allocation );
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

int64_t polcontract::cpu_rental_price(const uint64_t& days, const int64_t& price_per_day, const int64_t& amount) {

  uint128_t daily_cost      = safecast::mul( uint128_t(amount), uint128_t(price_per_day) );
  uint128_t expected_amount = mulDiv128( daily_cost, uint128_t(days), SCALE_FACTOR_1E8 );
  return safecast::safe_cast<int64_t>(expected_amount);

}

int64_t polcontract::cpu_rental_price_from_seconds(const uint64_t& seconds, const int64_t& price_per_day, const uint64_t& amount) {
  uint128_t daily_cost              = safecast::mul( uint128_t(amount), uint128_t(price_per_day) );
  uint128_t cost_scaled_by_seconds  = safecast::mul( uint128_t(seconds), daily_cost );
  uint128_t seconds_per_day_scaled  = safecast::mul( uint128_t(SECONDS_PER_DAY), SCALE_FACTOR_1E8 );
  uint128_t expected_amount         = safecast::div( cost_scaled_by_seconds, seconds_per_day_scaled );
  return safecast::safe_cast<int64_t>(expected_amount);
}

uint64_t polcontract::days_to_seconds(const uint64_t& days) {
  return uint64_t(SECONDS_PER_DAY * days);
}

void polcontract::deposit_liquidity_to_alcor(const liquidity_struct& lp_details) {

  transfer_tokens(ALCOR_CONTRACT, lp_details.poolA.amountToAdd, lp_details.poolA.contract, std::string("deposit"));
  transfer_tokens(ALCOR_CONTRACT, lp_details.poolB.amountToAdd, lp_details.poolB.contract, std::string("deposit"));

  action(permission_level{get_self(), "active"_n}, ALCOR_CONTRACT, "addliquid"_n,

    std::tuple{
      lp_details.poolId,
      _self,
      lp_details.poolA.amountToAdd,
      lp_details.poolB.amountToAdd,
      int32_t(-443580), //lowest tick, full range
      int32_t(443580), //highest tick, full range
      lp_details.poolA.minAsset,
      lp_details.poolB.minAsset,
      uint32_t(0), //deadline
    }

  ).send();
}

liquidity_struct polcontract::get_liquidity_info(config2 c, dapp_tables::global ds) {

  uint64_t  poolId        = c.lswax_wax_pool_id;
  auto      itr           = pools_t.require_find( poolId, ("could not locate pool id " + std::to_string(poolId) ).c_str() );
  uint128_t sqrtPriceX64  = itr->currSlot.sqrtPriceX64;

  token_a_or_b poolA, poolB;

  bool aIsWax = itr->tokenA.quantity.symbol == WAX_SYMBOL;

  poolA = aIsWax ? token_a_or_b{itr->tokenA.quantity.amount, WAX_SYMBOL, WAX_CONTRACT, ZERO_WAX, ZERO_WAX} : token_a_or_b{itr->tokenB.quantity.amount, LSWAX_SYMBOL, TOKEN_CONTRACT, ZERO_LSWAX, ZERO_LSWAX};
  poolB = !aIsWax ? token_a_or_b{itr->tokenA.quantity.amount, WAX_SYMBOL, WAX_CONTRACT, ZERO_WAX, ZERO_WAX} : token_a_or_b{itr->tokenB.quantity.amount, LSWAX_SYMBOL, TOKEN_CONTRACT, ZERO_LSWAX, ZERO_LSWAX};

  int64_t               real_lswax_price    = token_price( ds.swax_currently_backing_lswax.amount, ds.liquified_swax.amount );
  std::vector<int64_t>  alcor_prices        = sqrt64_to_price( sqrtPriceX64 );
  int64_t               alcors_lswax_price  = aIsWax ? alcor_prices[1] : alcor_prices[0];

  if ( real_lswax_price < calculate_asset_share( alcors_lswax_price, 105000000 ) //real price is <= 5% higher
       &&
       alcors_lswax_price < calculate_asset_share( real_lswax_price, 105000000 ) //real price <= 5% lower
     ) {
    return { true, alcors_lswax_price, real_lswax_price, aIsWax, poolA, poolB, poolId };
  } else {
    return { false, alcors_lswax_price, real_lswax_price, aIsWax, poolA, poolB, poolId };
  }

}

void polcontract::issue_refund_if_user_overpaid(const name& user, const asset& quantity, int64_t& amount_expected, int64_t& profit_made){
    if( quantity.amount > amount_expected ){
        int64_t amount_to_refund = safecast::sub( quantity.amount, amount_expected );

        if( amount_to_refund > 0 ){
            profit_made = safecast::sub( quantity.amount, amount_to_refund );
            transfer_tokens( user, asset( amount_to_refund, WAX_SYMBOL ), WAX_CONTRACT, "cpu rental refund from waxfusion.io - liquid staking protocol" );
        }
    }  
}

uint64_t polcontract::now() {
  return current_time_point().sec_since_epoch();
}

std::vector<std::string> polcontract::parse_memo(std::string memo) {
  std::string delim = "|";
  std::vector<std::string> words{};
  size_t pos = 0;
  while ((pos = memo.find(delim)) != std::string::npos) {
    words.push_back(memo.substr(0, pos));
    memo.erase(0, pos + delim.length());
  }
  return words;
}

void polcontract::stake_wax(const name& receiver, const int64_t& cpu_amount, const int64_t& net_amount) {
  action(permission_level{ _self, "active"_n}, SYSTEM_CONTRACT, "delegatebw"_n,
         std::tuple{ _self, receiver, asset( net_amount, WAX_SYMBOL ), asset( cpu_amount, WAX_SYMBOL ), false })
  .send();
}

uint128_t polcontract::seconds_to_days_1e6(const uint64_t& seconds) {
  return mulDiv128( uint128_t(seconds), SCALE_FACTOR_1E6, uint128_t(SECONDS_PER_DAY) );
}

//convert the sqrtPriceX64 from alcor into actual asset prices for tokenA and tokenB
std::vector<int64_t> polcontract::sqrt64_to_price(const uint128_t& sqrtPriceX64) {

    uint128_t priceX64      = mulDiv128( sqrtPriceX64, sqrtPriceX64, TWO_POW_64 );
    uint128_t P_tokenA_128  = mulDiv128( priceX64, SCALE_FACTOR_1E8, TWO_POW_64 );
    uint128_t P_tokenB_128  = safecast::div( SCALE_FACTOR_1E16, P_tokenA_128 );
    return { safecast::safe_cast<int64_t>(P_tokenA_128), safecast::safe_cast<int64_t>(P_tokenB_128) };

}

//get the price of tokenA A in terms of tokenB based on pool quantities
//since we are only working with lsWAX and WAX which both have 8 decimals, SCALE_FACTOR_1E8
int64_t polcontract::token_price(const int64_t& amount_A, const int64_t& amount_B) {

    if ( amount_A == amount_B ) return safecast::safe_cast<int64_t>(SCALE_FACTOR_1E8);

    return mulDiv( uint64_t(amount_A), safecast::safe_cast<uint64_t>(SCALE_FACTOR_1E8), uint128_t(amount_B) );
}

void polcontract::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo) {
  action(permission_level{get_self(), "active"_n}, contract, "transfer"_n, std::tuple{ get_self(), user, amount_to_send, memo}).send();
}

void polcontract::update_state() {
  state3 s = state_s_3.get();

  if ( now() >= s.next_day_end_time ) {

    uint64_t days_passed = ( now() - s.next_day_end_time + SECONDS_PER_DAY - 1 ) / SECONDS_PER_DAY;

    s.next_day_end_time += days_to_seconds( days_passed );
    state_s_3.set(s, _self);
  }
}


void polcontract::update_votes() {
  state3 s = state_s_3.get();

  if ( s.last_vote_time + days_to_seconds(1) <= now() ) {
    top21 t = top21_s.get();
    const name no_proxy;
    action(permission_level{get_self(), "active"_n}, "eosio"_n, "voteproducer"_n, std::tuple{ get_self(), no_proxy, t.block_producers }).send();

    s.last_vote_time = now();
    state_s_3.set(s, _self);
  }
}

void polcontract::validate_allocations( const int64_t& quantity, const std::vector<int64_t> allocations ) {
  int64_t sum = 0;

  for (int64_t a : allocations) {
    sum = safecast::add(sum, a);
  }

  check( sum <= quantity, "overallocation of funds" );
}

void polcontract::validate_liquidity_pair( const eosio::extended_asset& a, const eosio::extended_asset& b ) {
  auto is_wax = [](const auto & token) {
    return token.quantity.symbol == WAX_SYMBOL && token.contract == WAX_CONTRACT;
  };

  auto is_lswax = [](const auto & token) {
    return token.quantity.symbol == LSWAX_SYMBOL && token.contract == TOKEN_CONTRACT;
  };

  check( ( is_wax(a) && is_lswax(b) ) || ( is_lswax(a) && is_wax(b) ), "lp pair doesn't match WAX/LSWAX" );
}