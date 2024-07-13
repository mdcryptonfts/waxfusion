#pragma once

/** constructs an `active` permission object to pass to an inline action */

inline eosio::permission_level fusion::active_perm(){
    return eosio::permission_level{ _self, "active"_n };
}

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

void fusion::create_alcor_farm(const uint64_t& poolId, const symbol& token_symbol, const name& token_contract) {
  action(active_perm(), ALCOR_CONTRACT, "newincentive"_n,
         std::tuple{ _self, poolId, extended_asset(ZERO_LSWAX, TOKEN_CONTRACT), (uint32_t) LP_FARM_DURATION_SECONDS }
        ).send();
}

string fusion::cpu_stake_memo(const name& cpu_receiver, const uint64_t& epoch_timestamp) {
  return ("|stake_cpu|" + cpu_receiver.to_string() + "|" + std::to_string(epoch_timestamp) + "|").c_str();
}

void fusion::create_epoch(const global& g, const uint64_t& start_time, const name& cpu_wallet, const asset& wax_bucket) {
  epochs_t.emplace(_self, [&](auto & _e) {
    _e.start_time                       = start_time;
    _e.time_to_unstake                  = start_time + g.cpu_rental_epoch_length_seconds - days_to_seconds(3);
    _e.cpu_wallet                       = cpu_wallet;
    _e.wax_bucket                       = wax_bucket;
    _e.wax_to_refund                    = ZERO_WAX;
    _e.redemption_period_start_time     = start_time + g.cpu_rental_epoch_length_seconds;
    _e.redemption_period_end_time       = start_time + g.cpu_rental_epoch_length_seconds + days_to_seconds(2);
    _e.total_cpu_funds_returned         = ZERO_WAX;
    _e.total_added_to_redemption_bucket = ZERO_WAX;
  });
}

uint64_t fusion::days_to_seconds(const uint64_t& days) {
  return uint64_t(SECONDS_PER_DAY) * days;
}

/**
 * Adjusts redemption requests to make sure a user's sWAX balance is not overallocated
 * 
 * NOTE: When user's request redemptions, there is a waiting period. During this waiting period,
 * their sWAX balance remains in their account. We have to avoid cases where a user has made
 * multiple requests against the same balance, resulting in freezing the redemption pool funds
 * and making them unavailable to other users. Whenever a user's sWAX balance decreases, we
 * need to call this function to make sure that if they have redemption requests > their sWAX
 * balance, we adjust those requests until they are == the user's sWAX balance.
 * We start at the request farthest away first, so a user doesn't have to wait a long time
 * for their remaining redemption.
 * 
 * @param user - the wallet address of the user whose redemptions we are adjusting
 * @param swax_balance - `asset` containing the user's new sWAX balance
 */

void fusion::debit_user_redemptions_if_necessary(const name& user, const asset& swax_balance) {

  global          g                                 = global_s.get();
  requests_tbl    requests_t                        = requests_tbl(get_self(), user.value);
  asset           total_amount_awaiting_redemption  = ZERO_WAX;
  const uint64_t  first_epoch_to_check              = g.last_epoch_start_time - g.seconds_between_epochs;

  // We only need to check the 3 active epochs
  const std::vector<uint64_t> epochs_to_check = {
    first_epoch_to_check + ( g.seconds_between_epochs * 2 ),
    first_epoch_to_check + g.seconds_between_epochs,
    first_epoch_to_check
  };

  std::vector<uint64_t> pending_requests {};

  for (uint64_t ep : epochs_to_check) {
    auto epoch_itr  = epochs_t.find(ep);
    auto req_itr    = requests_t.find(ep);

    if ( epoch_itr != epochs_t.end() && req_itr != requests_t.end() ) {
        pending_requests.push_back(ep);
        total_amount_awaiting_redemption += req_itr->wax_amount_requested;
    }
  }

  if( total_amount_awaiting_redemption.amount <= swax_balance.amount ) return;

  int64_t amount_overdrawn = safecast::sub( total_amount_awaiting_redemption.amount, swax_balance.amount );

  for (uint64_t& pending : pending_requests) {
    auto epoch_itr  = epochs_t.require_find(pending, "error locating epoch");
    auto req_itr    = requests_t.require_find(pending, "error locating redemption request");

    if ( req_itr->wax_amount_requested.amount > amount_overdrawn ) {
      requests_t.modify(req_itr, same_payer, [&](auto & _r) {
        _r.wax_amount_requested.amount -= amount_overdrawn;
      });

      epochs_t.modify(epoch_itr, _self, [&](auto & _e) {
        _e.wax_to_refund.amount -= amount_overdrawn;
      });

      return;
    }

    else if ( req_itr->wax_amount_requested.amount == amount_overdrawn ) {
      epochs_t.modify(epoch_itr, _self, [&](auto & _e) {
        _e.wax_to_refund.amount -= amount_overdrawn;
      });

      requests_t.erase( req_itr );
      return;
    }

    else {

      amount_overdrawn = safecast::sub( amount_overdrawn, req_itr->wax_amount_requested.amount );

      epochs_t.modify(epoch_itr, _self, [&](auto & _e) {
        _e.wax_to_refund -= req_itr->wax_amount_requested;
      });

      requests_t.erase( req_itr );
    }
  }
}

eosio::name fusion::get_next_cpu_contract(global& g) {

  auto itr = std::find( g.cpu_contracts.begin(), g.cpu_contracts.end(), g.current_cpu_contract );
  check( itr != g.cpu_contracts.end(), "error locating cpu contract" );

  auto next_cpu_index     = ( std::distance(g.cpu_contracts.begin(), itr) + 1 ) % g.cpu_contracts.size();
  name next_cpu_contract  = g.cpu_contracts[next_cpu_index];

  check(next_cpu_contract != g.current_cpu_contract, "next cpu contract cant be the same as the current contract");

  return next_cpu_contract;
}

/**
 * Calculates how many seconds a CPU rental will be
 * 
 * NOTE: Since CPU rentals from this contract are linked to epochs,
 * which have pre-determined timeframes, a renter tells us which epoch
 * they want to rent from, and then we dynamically calculate the price
 * (based on time remaining in that epoch).
 * 
 * @param g - `global` singleton
 * @param epoch_id_to_rent_from - which epoch the renter wants to rent from
 * 
 * @return uint64_t - amount of seconds remaining in the epoch
 */

uint64_t fusion::get_seconds_to_rent_cpu( global& g, const uint64_t& epoch_id_to_rent_from ) {
  
  uint64_t seconds_into_current_epoch = now() - g.last_epoch_start_time;
  uint64_t seconds_to_rent;

  if ( epoch_id_to_rent_from == g.last_epoch_start_time + g.seconds_between_epochs ) {
    // Renting from epoch 3 (hasnt started yet)
    seconds_to_rent = days_to_seconds(18) - seconds_into_current_epoch;

  } else if ( epoch_id_to_rent_from == g.last_epoch_start_time ) {
    // Renting from epoch 2 (started most recently)
    seconds_to_rent = days_to_seconds(11) - seconds_into_current_epoch;

  } else if ( epoch_id_to_rent_from == g.last_epoch_start_time - g.seconds_between_epochs ) {
    // Renting from epoch 1 (oldest) and we need to make sure it's less than 11 days old
    check( seconds_into_current_epoch < days_to_seconds(4), "it is too late to rent from this epoch, please rent from the next one" );

    // If we reached here, minimum PAYMENT is 1 full day (even if rental is less than 1 day)
    seconds_to_rent = days_to_seconds(4) - seconds_into_current_epoch < days_to_seconds(1) ? days_to_seconds(1) : days_to_seconds(4) - seconds_into_current_epoch;

  } else {
    check( false, "you are trying to rent from an invalid epoch" );
  }

  return seconds_to_rent;
}


vector<string> fusion::get_words(string memo) {
  string delim = "|";
  vector<string> words{};
  size_t pos = 0;
  while ((pos = memo.find(delim)) != string::npos) {
    words.push_back(memo.substr(0, pos));
    memo.erase(0, pos + delim.length());
  }
  return words;
}

/**
 * Checks if `user` is one of the `admin_wallets` in `global` singleton
 * 
 * NOTE: While this contract is under multisig, there are some less critical
 * actions that can be called by accounts that we've given elevated permissions to.
 * 
 * @param g - `global` singleton
 * @param user - the wallet address to look for in the `admin_wallets` vector
 * 
 * @return bool - whether or not the address was found
 */

bool fusion::is_an_admin(global& g, const name& user) {
  return std::find(g.admin_wallets.begin(), g.admin_wallets.end(), user) != g.admin_wallets.end();
}

/**
 * Checks if `contract` is one of the `cpu_contracts` in `global` singleton
 * 
 * @param g - `global` singleton
 * @param contract - the wallet address to look for in the `cpu_contracts` vector
 * 
 * @return bool - whether or not the address was found
 */

bool fusion::is_cpu_contract(global& g, const name& contract) {
  return std::find( g.cpu_contracts.begin(), g.cpu_contracts.end(), contract) != g.cpu_contracts.end();
}

bool fusion::is_lswax_or_wax(const symbol& symbol, const name& contract)
{
  return (symbol == LSWAX_SYMBOL && contract == TOKEN_CONTRACT) || (symbol == WAX_SYMBOL && contract == WAX_CONTRACT);
}

void fusion::issue_lswax(const int64_t& amount, const name& receiver) {
  action(active_perm(), TOKEN_CONTRACT, "issue"_n, std::tuple{ _self, receiver, asset(amount, LSWAX_SYMBOL), std::string("issuing lsWAX to liquify") }).send();
}

void fusion::issue_swax(const int64_t& amount) {
  action(active_perm(), TOKEN_CONTRACT, "issue"_n, std::tuple{ _self, _self, asset(amount, SWAX_SYMBOL), std::string("issuing sWAX for staking") }).send();
}

bool fusion::memo_is_expected(const string& memo) {

  if ( memo == "instant redeem" || memo == "rebalance" || memo == "wax_lswax_liquidity" || memo == "stake" || memo == "unliquify" || memo == "waxfusion_revenue" || memo == "cpu rental return" || memo == "lp_incentives" ) {
    return true;
  }

  string          memo_copy   = memo;
  vector<string>  words       = get_words(memo_copy);

  return ( words[1] == "rent_cpu" || words[1] == "unliquify_exact" );
}

inline uint64_t fusion::now() {
  return current_time_point().sec_since_epoch();
}

void fusion::retire_lswax(const int64_t& amount) {
  action(active_perm(), TOKEN_CONTRACT, "retire"_n, std::tuple{ asset(amount, LSWAX_SYMBOL), std::string("retiring lsWAX to unliquify")}).send();
}

void fusion::retire_swax(const int64_t& amount) {
  action(active_perm(), TOKEN_CONTRACT, "retire"_n, std::tuple{ asset(amount, SWAX_SYMBOL), std::string("retiring sWAX for redemption")}).send();
}

inline void fusion::sync_epoch(global& g) {

  uint64_t next_epoch_start_time = g.last_epoch_start_time + g.seconds_between_epochs;

  // There is an edge case possible where if the contract has no interactions over the course
  // of an entire epoch, then the epoch would never get created. This is solved by
  // using a while loop to create any missing epochs if they got skipped
  while ( now() >= next_epoch_start_time ) {

    name next_cpu_contract = get_next_cpu_contract( g );

    g.last_epoch_start_time = next_epoch_start_time;
    g.current_cpu_contract  = next_cpu_contract;

    if ( epochs_t.find(next_epoch_start_time) == epochs_t.end() ) {
      create_epoch( g, next_epoch_start_time, next_cpu_contract, ZERO_WAX );
    }

    next_epoch_start_time += g.seconds_between_epochs;
  }
}

void fusion::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const string& memo) {
  action(active_perm(), contract, "transfer"_n, std::tuple{ get_self(), user, amount_to_send, memo}).send();
}

/**
 * Sums up a vector of `allocations` and makes sure the total <= `quantity`
 * 
 * Throws if sum is > quantity
 * 
 * @param quantity - `int64_t` maximum acceptable sum of `allocations` parts
 * @param allocations - `vector<int64_t>` parts to calculate the sum of
 */

void fusion::validate_allocations( const int64_t& quantity, const vector<int64_t> allocations ) {
  int64_t sum = 0;

  for (int64_t a : allocations) {
    sum = safecast::add(sum, a);
  }

  check( sum <= quantity, "overallocation of funds" );
}