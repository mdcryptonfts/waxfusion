#pragma once

void polcontract::add_liquidity( state3& s, liquidity_struct& lp_details ) {

  //Maximum possible based on buckets
  int64_t max_wax_possible = mulDiv( uint64_t(s.wax_bucket.amount), safecast::safe_cast<uint64_t>(SCALE_FACTOR_1E8), uint128_t(lp_details.alcors_lswax_price) );
  int64_t max_lswax_possible = s.lswax_bucket.amount;

  // Determine the limiting factor
  int64_t wax_to_add, lswax_to_add;
  if (max_wax_possible <= max_lswax_possible) {
    //WAX is the limiting factor, or the weights are equal
    //so we will add the full wax bucket to LP
    wax_to_add = s.wax_bucket.amount;
    lswax_to_add = mulDiv( uint64_t(wax_to_add), safecast::safe_cast<uint64_t>(SCALE_FACTOR_1E8), uint128_t(lp_details.alcors_lswax_price) );

  } else {
    // LSWAX is the limiting factor so we will add the full bucket to alcor
    lswax_to_add = s.lswax_bucket.amount;
    wax_to_add = mulDiv( uint64_t(lswax_to_add), uint64_t(lp_details.alcors_lswax_price), SCALE_FACTOR_1E8 );
  }

  //determine whether tokenA or tokenB is wax, and populate the data accordingly so we can construct a transaction for alcor
  lp_details.poolA.amountToAdd = lp_details.aIsWax ? asset( wax_to_add, WAX_SYMBOL ) : asset( lswax_to_add, LSWAX_SYMBOL );
  lp_details.poolB.amountToAdd = !lp_details.aIsWax ? asset( wax_to_add, WAX_SYMBOL ) : asset( lswax_to_add, LSWAX_SYMBOL );

  //set the minimum amounts to add for each asset at 99.5% to allow minor slippage
  lp_details.poolA.minAsset = lp_details.aIsWax ? asset( calculate_asset_share( wax_to_add, 99500000), WAX_SYMBOL ) : asset( calculate_asset_share( lswax_to_add, 99500000), LSWAX_SYMBOL );
  lp_details.poolB.minAsset = !lp_details.aIsWax ? asset( calculate_asset_share( wax_to_add, 99500000), WAX_SYMBOL ) : asset( calculate_asset_share( lswax_to_add, 99500000), LSWAX_SYMBOL );


  //debit the amounts from wax bucket and lswax bucket
  if (lp_details.aIsWax) {
    //if tokenA is wax and tokenB is lswax

    //validate amounts before proceeding
    check( s.wax_bucket.amount >= lp_details.poolA.amountToAdd.amount, "overallocation error" );
    check( s.lswax_bucket.amount >= lp_details.poolB.amountToAdd.amount, "overallocation error" );

    //update the state singleton and debit the buckets
    s.wax_bucket.amount = safeSubInt64( s.wax_bucket.amount, lp_details.poolA.amountToAdd.amount );
    s.lswax_bucket.amount = safeSubInt64( s.lswax_bucket.amount, lp_details.poolB.amountToAdd.amount );
  } else {
    //if tokenB is wax and tokenA is lswax

    //validate amounts before proceeding
    check( s.wax_bucket.amount >= lp_details.poolB.amountToAdd.amount, "overallocation error" );
    check( s.lswax_bucket.amount >= lp_details.poolA.amountToAdd.amount, "overallocation error" );

    //validate amounts before proceeding
    s.wax_bucket.amount = safeSubInt64( s.wax_bucket.amount, lp_details.poolB.amountToAdd.amount );
    s.lswax_bucket.amount = safeSubInt64( s.lswax_bucket.amount, lp_details.poolA.amountToAdd.amount );
  }



  //make the transfers to alcor and call the `addliquid` action
  deposit_liquidity_to_alcor(lp_details);
}

int64_t polcontract::cpu_rental_price(const uint64_t& days, const int64_t& price_per_day, const int64_t& amount) {
  //formula is days * amount * price_per_day
  uint128_t daily_cost = safeMulUInt128( uint128_t(amount), uint128_t(price_per_day) );
  uint128_t expected_amount_scaled = safeMulUInt128( daily_cost, uint128_t(days) );
  uint128_t expected_amount = safeDivUInt128( expected_amount_scaled, SCALE_FACTOR_1E8 );
  return int64_t(expected_amount);
}

int64_t polcontract::cpu_rental_price_from_seconds(const uint64_t& seconds, const int64_t& price_per_day, const uint64_t& amount) {
  uint128_t daily_cost = safeMulUInt128( uint128_t(amount), uint128_t(price_per_day) );
  uint128_t cost_scaled_by_seconds = safeMulUInt128( uint128_t(seconds), daily_cost );
  uint128_t seconds_per_day_scaled = safeMulUInt128( uint128_t(SECONDS_PER_DAY), SCALE_FACTOR_1E8 );
  uint128_t expected_amount = safeDivUInt128( cost_scaled_by_seconds, seconds_per_day_scaled );
  return int64_t(expected_amount);
}

uint64_t polcontract::days_to_seconds(const uint64_t& days) {
  return uint64_t(SECONDS_PER_DAY * days);
}

void polcontract::deposit_liquidity_to_alcor(const liquidity_struct& lp_details) {
  /**
  * call the necessary actions:
  * - transfer wax to alcor "deposit"
  * - transfer lswax to alcor "deposit"
  * - alcor `addliquid`
  */

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

liquidity_struct polcontract::get_liquidity_info(config2 c, dapp_tables::state ds) {
  //get the pool ID for our Alcor pair from our config singleton
  uint64_t poolId = c.lswax_wax_pool_id;

  //make sure we can find the pair on alcor
  auto itr = pools_t.require_find( poolId, ("could not locate pool id " + std::to_string(poolId) ).c_str() );
  uint128_t sqrtPriceX64 = itr->currSlot.sqrtPriceX64;

  //initialize default structs for the 2 sides of the pair
  token_a_or_b poolA, poolB;

  //if tokenA on alcor pair is WAX, set flag to true
  bool aIsWax = (itr->tokenA.quantity.symbol == WAX_SYMBOL);

  //populate the structs appropriately based on whether tokenA or tokenB is wax
  poolA = aIsWax ? token_a_or_b{itr->tokenA.quantity.amount, WAX_SYMBOL, WAX_CONTRACT, ZERO_WAX, ZERO_WAX} : token_a_or_b{itr->tokenB.quantity.amount, LSWAX_SYMBOL, TOKEN_CONTRACT, ZERO_LSWAX, ZERO_LSWAX};
  poolB = !aIsWax ? token_a_or_b{itr->tokenA.quantity.amount, WAX_SYMBOL, WAX_CONTRACT, ZERO_WAX, ZERO_WAX} : token_a_or_b{itr->tokenB.quantity.amount, LSWAX_SYMBOL, TOKEN_CONTRACT, ZERO_LSWAX, ZERO_LSWAX};

  //get the lswax price from the dapp.fusion contract
  //this allows us to compare against alcors price and determine if alcors price is acceptable
  //to avoid potential losses by depositing liquidity at an unreasonable price
  int64_t real_lswax_price = token_price( ds.swax_currently_backing_lswax.amount, ds.liquified_swax.amount );

  //get alcor's price, so we can compare it to the real price
  std::vector<int64_t> alcor_prices = sqrt64_to_price( sqrtPriceX64 );
  int64_t alcors_lswax_price = aIsWax ? alcor_prices[1] : alcor_prices[0];

  /** to determine if alcor's price is acceptable, one of the following must be true
   *  - lswax is cheaper on alcor than the real price, but the difference is <= 5%
   *  - lswax is the same price as the real price
   *  - lswax is <= 5% more than the real price
   *  if none of these are true, we will save the wax for now and check again during next revenue distribution
   *  if 1 week goes by without the price being acceptable, we will move funds into the CPU rental pool instead
   */

  if ( real_lswax_price < calculate_asset_share( alcors_lswax_price, 105000000 ) //real price is <= 5% higher
       &&
       alcors_lswax_price < calculate_asset_share( real_lswax_price, 105000000 ) //real price <= 5% lower
     ) {
    return { true, alcors_lswax_price, real_lswax_price, aIsWax, poolA, poolB, poolId };
  } else {
    return { false, alcors_lswax_price, real_lswax_price, aIsWax, poolA, poolB, poolId };
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
  uint128_t seconds_1e6 = safeMulUInt128( uint128_t(seconds), SCALE_FACTOR_1E6 );
  return safeDivUInt128( uint128_t(seconds_1e6), uint128_t(SECONDS_PER_DAY) );
}

void polcontract::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo) {
  action(permission_level{get_self(), "active"_n}, contract, "transfer"_n, std::tuple{ get_self(), user, amount_to_send, memo}).send();
}

void polcontract::update_state() {
  state3 s = state_s_3.get();

  if ( now() >= s.next_day_end_time ) {

    //calculate the amount of days that have passed
    //this is the equivalent of using ceil(), but with integers rather
    //than needing to use doubles
    uint64_t days_passed = ( now() - s.next_day_end_time + SECONDS_PER_DAY - 1 ) / SECONDS_PER_DAY;

    s.next_day_end_time += days_to_seconds( days_passed );
    state_s_3.set(s, _self);
  }
}


void polcontract::update_votes() {
  state3 s = state_s_3.get();

  if ( s.last_vote_time + days_to_seconds(1) <= now() ) {
    top21 t = top21_s.get();

    const eosio::name no_proxy;
    action(permission_level{get_self(), "active"_n}, "eosio"_n, "voteproducer"_n, std::tuple{ get_self(), no_proxy, t.block_producers }).send();

    s.last_vote_time = now();
    state_s_3.set(s, _self);
  }
}

void polcontract::validate_allocations( const int64_t& quantity, const std::vector<int64_t> allocations ) {
  int64_t sum = 0;

  for (int64_t a : allocations) {
    sum = safeAddInt64(sum, a);
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