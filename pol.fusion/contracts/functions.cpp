#pragma once


void polcontract::add_liquidity( state3& s, liquidity_struct& lp_details ){

  //scale the wax bucket by e18
  uint128_t wax_bucket_e18 = safeMulUInt128( uint128_t(s.wax_bucket.amount), SCALE_FACTOR_1E18 );

  //make sure its safe to do division
  check( wax_bucket_e18 > lp_details.alcors_lswax_price, "can not safely divide the bucket" );

  //Maximum possible based on buckets
  uint128_t max_wax_possible = safeDivUInt128( wax_bucket_e18, lp_details.alcors_lswax_price );
  uint128_t max_lswax_possible = uint128_t(s.lswax_bucket.amount);

  // Determine the limiting factor
  uint128_t wax_to_add, lswax_to_add;
  if (max_wax_possible <= max_lswax_possible) {
      // WAX is the limiting factor so we just add the full bucket to LP
      wax_to_add = uint128_t(s.wax_bucket.amount);

      //scale wax_to_add before division
      uint128_t wax_to_add_e18 = safeMulUInt128( wax_to_add, SCALE_FACTOR_1E18 );

      //make sure division is safe
      check( wax_to_add_e18 > lp_details.alcors_lswax_price, "can not safely divide to calculate LP" );

      //calculate the amount of lswax to pair with the wax bucket
      lswax_to_add = safeDivUInt128( wax_to_add_e18, lp_details.alcors_lswax_price );

  } else {
      // LSWAX is the limiting factor so we will add the full bucket to alcor
      lswax_to_add = uint128_t(s.lswax_bucket.amount);

      /** if alcor price < the max_scale_factor for lswax_to_add,
       *  multiplying it the 2 is acceptable
       *  otherwise there will be overflow, so we can run a loop
       *  to scale down until the size is acceptable
       * 
       *  we use a bool to determine if we scaled down or not
       *  otherwise we would be dividing the end result by too large of 
       *  a scale factor, unnecessarily which would cause incorrect math
       */

      uint128_t scale_factor = max_scale_without_room( lswax_to_add );

      int count = 0;
      bool scaled_down_alcor = false;

      while(lp_details.alcors_lswax_price > scale_factor && count < 12 ){
        scaled_down_alcor = true;
        lp_details.alcors_lswax_price /= 10;
        count++;

        if(count == 11){
          check(false, "can not get pool price");
        }
        
      }

      //multiply wax_to_add by alcors price before division, to determine the correct amount to add
      //then scale it down by the scale factor
      uint128_t wax_to_add_scaled = safeMulUInt128( lswax_to_add, lp_details.alcors_lswax_price );
      wax_to_add = scaled_down_alcor ? safeDivUInt128( wax_to_add_scaled, scale_factor ) : safeDivUInt128( wax_to_add_scaled, SCALE_FACTOR_1E18 );
  }

  //determine whether tokenA or tokenB is wax, and populate the data accordingly so we can construct a transaction for alcor
  lp_details.poolA.amountToAdd = lp_details.aIsWax ? asset( int64_t(wax_to_add), WAX_SYMBOL ) : asset( int64_t(lswax_to_add), LSWAX_SYMBOL );
  lp_details.poolB.amountToAdd = !lp_details.aIsWax ? asset( int64_t(wax_to_add), WAX_SYMBOL ) : asset( int64_t(lswax_to_add), LSWAX_SYMBOL );

  //set the minimum amounts to add for each asset at 99.5% to allow minor slippage
  lp_details.poolA.minAsset = lp_details.aIsWax ? asset( calculate_asset_share( int64_t(wax_to_add), 99500000), WAX_SYMBOL ) : asset( calculate_asset_share( int64_t(lswax_to_add), 99500000), LSWAX_SYMBOL );
  lp_details.poolB.minAsset = !lp_details.aIsWax ? asset( calculate_asset_share( int64_t(wax_to_add), 99500000), WAX_SYMBOL ) : asset( calculate_asset_share( int64_t(lswax_to_add), 99500000), LSWAX_SYMBOL );




  //debit the amounts from wax bucket and lswax bucket
  if(lp_details.aIsWax){
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

  return;

}

int64_t polcontract::cpu_rental_price(const uint64_t& days, const int64_t& price_per_day, const int64_t& amount){
  //formula is days * amount * price_per_day
  uint128_t daily_cost = safeMulUInt128( uint128_t(amount), uint128_t(price_per_day) );
  uint128_t expected_amount_scaled = safeMulUInt128( daily_cost, uint128_t(days) );
  uint128_t expected_amount = safeDivUInt128( expected_amount_scaled, SCALE_FACTOR_1E8 );
  return int64_t(expected_amount);
}

int64_t polcontract::cpu_rental_price_from_seconds(const uint64_t& seconds, const int64_t& price_per_day, const uint64_t& amount){
  uint128_t daily_cost = safeMulUInt128( uint128_t(amount), uint128_t(price_per_day) );
  uint128_t cost_scaled_by_seconds = safeMulUInt128( uint128_t(seconds), daily_cost );
  uint128_t seconds_per_day_scaled = safeMulUInt128( uint128_t(SECONDS_PER_DAY), SCALE_FACTOR_1E8 );
  uint128_t expected_amount = safeDivUInt128( cost_scaled_by_seconds, seconds_per_day_scaled );
  return int64_t(expected_amount);
}

uint64_t polcontract::days_to_seconds(const uint64_t& days){
  return uint64_t(SECONDS_PER_DAY * days);
}

void polcontract::deposit_liquidity_to_alcor(const liquidity_struct& lp_details){
  /**
  * call the necessary actions:
  * - transfer wax to alcor "deposit"
  * - transfer lswax to alcor "deposit" 
  * - alcor `addliquid`
  */

  transfer_tokens(ALCOR_CONTRACT, lp_details.poolA.amountToAdd, lp_details.poolA.contract, std::string("deposit"));
  transfer_tokens(ALCOR_CONTRACT, lp_details.poolB.amountToAdd, lp_details.poolB.contract, std::string("deposit"));

  action(permission_level{get_self(), "active"_n}, ALCOR_CONTRACT,"addliquid"_n,
    
    std::tuple{ 
      lp_details.poolId,
      _self,
      lp_details.poolA.amountToAdd,
      lp_details.poolB.amountToAdd,
      int32_t(-443580),
      int32_t(443580),
      lp_details.poolA.minAsset,
      lp_details.poolB.minAsset,
      uint32_t(0),
    }

  ).send();

  return;
}

liquidity_struct polcontract::get_liquidity_info(config2 c, dapp_tables::state ds){
  //get the pool ID for our Alcor pair from our config singleton
  uint64_t poolId = c.lswax_wax_pool_id;

  //make sure we can find the pair on alcor
  auto itr = pools_t.require_find( poolId, ("could not locate pool id " + std::to_string(poolId) ).c_str() );

  //initialize default structs for the 2 sides of the pair
  token_a_or_b poolA, poolB;

  //if tokenA on alcor pair is WAX, set flag to true
  bool aIsWax = (itr->tokenA.quantity.symbol == WAX_SYMBOL);

  //populate the structs appropriately based on whether tokenA or tokenB is wax
  poolA = aIsWax ? token_a_or_b{itr->tokenA.quantity.amount, WAX_SYMBOL, WAX_CONTRACT, ZERO_WAX, ZERO_WAX} : token_a_or_b{itr->tokenB.quantity.amount, LSWAX_SYMBOL, TOKEN_CONTRACT, ZERO_LSWAX, ZERO_LSWAX};
  poolB = !aIsWax ? token_a_or_b{itr->tokenA.quantity.amount, WAX_SYMBOL, WAX_CONTRACT, ZERO_WAX, ZERO_WAX} : token_a_or_b{itr->tokenB.quantity.amount, LSWAX_SYMBOL, TOKEN_CONTRACT, ZERO_LSWAX, ZERO_LSWAX};

  //initialize variables to store the amounts in each side of the pair,
  //according to which token is wax and which is lswax
  int64_t pool_wax = aIsWax ? poolA.quantity : poolB.quantity;
  int64_t pool_lswax = !aIsWax ? poolA.quantity : poolB.quantity;

  //get the scaled wax price from the dapp.fusion contract
  //this allows us to compare against alcors price and determine if alcors price is acceptable
  //to avoid potential losses by depositing liquidity at an unreasonable price
  uint128_t real_lswax_price = pool_ratio_1e18( ds.swax_currently_backing_lswax.amount, ds.liquified_swax.amount );

  //if there is 0 tokens in each side of the pool, this is acceptable because its 1:1 ratio
  //allowing us to deposit initial liquidity
  //after that point, we want to make sure there is at least something in each side of the pair
  //this also helps validate quantities before calling pool_ratio_1e18 below
  if( (pool_wax < 10 || pool_lswax < 10) && ( pool_wax > 0 || pool_lswax > 0 ) ){
    return { false, 0, real_lswax_price, false, poolA, poolB, poolId };
  }

  //get the scaled version of alcor's price, so we can compare it to the real price
  uint128_t alcors_lswax_price = pool_ratio_1e18( pool_wax, pool_lswax );

  /** to determine if alcor's price is acceptable, one of the following must be true
   *  - lswax is cheaper on alcor than the real price, but the difference is <= 5%
   *  - lswax is the same price as the real price
   *  - lswax is <= 5% more than the real price
   *  if none of these are true, we will save the wax for now and check again during next revenue distribution
   *  if 1 week goes by without the price being acceptable, we will move funds into the CPU rental pool instead
   *  
   *  since the numbers are already scaled so high, multiplying more becomes difficult
   *  rather than adding 5% to real_lswax_price, we'll check if our price > 95.238095% of alcors price
   *  which is equivalent to checking if our price is < ( alcors_price * 1.05 ) 
   */

  if( real_lswax_price > calculate_share_from_e18( alcors_lswax_price, 95238095 ) //real price is <= higher
      &&
      alcors_lswax_price > calculate_share_from_e18( real_lswax_price, 95238095 ) //real price <= 5% lower
    ){
    return { true, alcors_lswax_price, real_lswax_price, aIsWax, poolA, poolB, poolId };
  } else {
    return { false, alcors_lswax_price, real_lswax_price, aIsWax, poolA, poolB, poolId };
  }

}

uint64_t polcontract::now(){
  return current_time_point().sec_since_epoch();
}

std::vector<std::string> polcontract::parse_memo(std::string memo){
  std::string delim = "|";
  std::vector<std::string> words{};
  size_t pos = 0;
  while ((pos = memo.find(delim)) != std::string::npos) {
    words.push_back(memo.substr(0, pos));
    memo.erase(0, pos + delim.length());
  }
  return words;
}

void polcontract::stake_wax(const name& receiver, const int64_t& cpu_amount, const int64_t& net_amount){
  action(permission_level{ _self, "active"_n}, SYSTEM_CONTRACT, "delegatebw"_n,
    std::tuple{ _self, receiver, asset( net_amount, WAX_SYMBOL ), asset( cpu_amount, WAX_SYMBOL ), false })
  .send();
  return;
}

uint128_t polcontract::seconds_to_days_1e6(const uint64_t& seconds){
  uint128_t seconds_1e6 = safeMulUInt128( uint128_t(seconds), SCALE_FACTOR_1E6 );
  return safeDivUInt128( uint128_t(seconds_1e6), uint128_t(SECONDS_PER_DAY) );
}

void polcontract::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo){
  action(permission_level{get_self(), "active"_n}, contract,"transfer"_n,std::tuple{ get_self(), user, amount_to_send, memo}).send();
  return;
}

void polcontract::update_state(){
  state3 s = state_s_3.get();

  if( now() >= s.next_day_end_time ){

    //calculate the amount of days that have passed
    //this is the equivalent of using ceil(), but with integers rather
    //than needing to use doubles
    uint64_t days_passed = ( now() - s.next_day_end_time + SECONDS_PER_DAY - 1 ) / SECONDS_PER_DAY;

    s.next_day_end_time += days_to_seconds( days_passed );
    state_s_3.set(s, _self);
  }
}


void polcontract::update_votes(){
  state3 s = state_s_3.get();

  if( s.last_vote_time + days_to_seconds(1) <= now() ){
    top21 t = top21_s.get();

    const eosio::name no_proxy;
    action(permission_level{get_self(), "active"_n}, "eosio"_n,"voteproducer"_n,std::tuple{ get_self(), no_proxy, t.block_producers }).send();

    s.last_vote_time = now();
    state_s_3.set(s, _self);
  }

  return;
}

void polcontract::validate_allocations( const int64_t& quantity, const int64_t& lswax_alloc, const int64_t& wax_alloc, const int64_t& rental_alloc ){
    int64_t check_1 = safeAddInt64( lswax_alloc, wax_alloc );
    int64_t check_2 = safeAddInt64( check_1, rental_alloc );
    check( check_2 <= quantity, "overallocation of funds" );
    return;
}