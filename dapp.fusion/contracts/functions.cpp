#pragma once

void fusion::create_alcor_farm(const uint64_t& poolId, const eosio::symbol& token_symbol, const eosio::name& token_contract){
  action(permission_level{get_self(), "active"_n}, ALCOR_CONTRACT,"newincentive"_n,
      std::tuple{ get_self(), poolId, eosio::extended_asset(ZERO_LSWAX, TOKEN_CONTRACT), (uint32_t) LP_FARM_DURATION_SECONDS}
    ).send();
  return;
}

void fusion::create_epoch(const config3& c, const uint64_t& start_time, const name& cpu_wallet, const asset& wax_bucket){
  epochs_t.emplace(_self, [&](auto &_e){
    _e.start_time = start_time;
    /* unstake 3 days before epoch ends */
    _e.time_to_unstake = start_time + c.cpu_rental_epoch_length_seconds - days_to_seconds(3);
    _e.cpu_wallet = cpu_wallet;
    _e.wax_bucket = wax_bucket;
    _e.wax_to_refund = ZERO_WAX;
    /* redemption starts at the end of the epoch, ends 48h later */
    _e.redemption_period_start_time = start_time + c.cpu_rental_epoch_length_seconds;
    _e.redemption_period_end_time = start_time + c.cpu_rental_epoch_length_seconds + days_to_seconds(2);
    _e.total_cpu_funds_returned = ZERO_WAX;
    _e.total_added_to_redemption_bucket = ZERO_WAX;
  }); 
  return;  
}

void fusion::create_snapshot(const state& s, const int64_t& swax_earning_alloc_i64, const int64_t& swax_autocompounding_alloc_i64, 
    const int64_t& pol_alloc_i64, const int64_t& eco_alloc_i64, const int64_t& amount_to_distribute)
{
  snaps_t.emplace(_self, [&](auto &_snap){
    _snap.timestamp = s.next_distribution;
    _snap.swax_earning_bucket = asset(swax_earning_alloc_i64, WAX_SYMBOL);
    _snap.lswax_autocompounding_bucket = asset(swax_autocompounding_alloc_i64, WAX_SYMBOL);
    _snap.pol_bucket = asset(pol_alloc_i64, WAX_SYMBOL);
    _snap.ecosystem_bucket = asset(eco_alloc_i64, WAX_SYMBOL);
    _snap.total_distributed = asset(amount_to_distribute, WAX_SYMBOL);  
    _snap.total_swax_earning = s.swax_currently_earning;
  });  
  return;
}

void fusion::credit_total_claimable_wax(const eosio::asset& amount_to_credit){
  if(amount_to_credit.amount > 0 && amount_to_credit.amount <= MAX_ASSET_AMOUNT_U64){
    state3 s3 = state_s_3.get();

    s3.total_claimable_wax.amount = safeAddInt64( s3.total_claimable_wax.amount, amount_to_credit.amount );

    state_s_3.set(s3, _self);
  }
  return;
}

void fusion::debit_total_claimable_wax(state3& s3, const eosio::asset& amount_to_debit){
  if(amount_to_debit.amount > 0 && amount_to_debit.amount <= MAX_ASSET_AMOUNT_U64){
    s3.total_claimable_wax.amount = safeSubInt64( s3.total_claimable_wax.amount, amount_to_debit.amount );
  }
  return;
}

/** debit_user_redemptions_if_necessary
 *  if the user had a pending redemption request and they do anything that results in 
 *  their swax balance being decreased, we need to make sure that their pending redemption
 *  requests are updated to be == their new swax balance, otherwise they could prevent 
 *  other users from redeeming by tying up the redemption pool with requests that they 
 *  cant cover
 */

void fusion::debit_user_redemptions_if_necessary(const name& user, const asset& swax_balance){
  //need to know which epochs to check
  state s = states.get();
  config3 c = config_s_3.get();

  requests_tbl requests_t = requests_tbl(get_self(), user.value);

  //initialize an empty asset before we tally up their requests
  eosio::asset total_amount_awaiting_redemption = eosio::asset( 0, WAX_SYMBOL );

  //figure out the first epoch we need to check, and then use it to determine the other 2
  //epochs that need to be checked
  const uint64_t first_epoch_to_check = s.last_epoch_start_time - c.seconds_between_epochs;

  //we only need to check the 3 active epochs
  //the vector will start in reverse order, so if the user has multiple requests,
  //we will debit from the one that is farthest away first. this way they dont
  //have to wait a long time to get their redemption
  const std::vector<uint64_t> epochs_to_check = {
    first_epoch_to_check + ( c.seconds_between_epochs * 2 ),
    first_epoch_to_check + c.seconds_between_epochs,
    first_epoch_to_check
  };    

  //check if they have any pending requests, and if so, tally up the amounts
  for(uint64_t ep : epochs_to_check){
    auto epoch_itr = epochs_t.find(ep);

    //if the epoch exists, check for a redemption, otherwise do nothing
    if(epoch_itr != epochs_t.end()){

      auto req_itr = requests_t.find(ep); 

      if(req_itr != requests_t.end()){
        //there is a pending request

        //add the requested amount to total_amount_awaiting_redemption
        total_amount_awaiting_redemption.amount = safeAddInt64( total_amount_awaiting_redemption.amount, req_itr->wax_amount_requested.amount );
      }
    } 
  }  

  //if the user has pending requests that are > their swax balance, we have to debit their pending redemptions
  if( total_amount_awaiting_redemption.amount > swax_balance.amount ){
    
    //calculate how much we need to remove from their redemptions
    int64_t amount_overdrawn_i64 = safeSubInt64( total_amount_awaiting_redemption.amount, swax_balance.amount );

    //loop through the 3 epochs and update request amounts until they are no longer overdrawn
    for(uint64_t ep : epochs_to_check){
      auto epoch_itr = epochs_t.find(ep);

      //if the epoch doesn't exist, skip it
      if(epoch_itr != epochs_t.end()){

        auto req_itr = requests_t.find(ep); 

        if(req_itr != requests_t.end()){
          //there is a pending request

          //if the amount requested is > amount_overdrawn_i64, only subtract the difference, and break
          if( req_itr->wax_amount_requested.amount > amount_overdrawn_i64 ){
            requests_t.modify(req_itr, same_payer, [&](auto &_r){
              _r.wax_amount_requested.amount = safeSubInt64( _r.wax_amount_requested.amount, amount_overdrawn_i64 );
            });

            //update the epoch so it properly tracks how much pending requests are tied to it
            epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
              _e.wax_to_refund.amount = safeSubInt64( _e.wax_to_refund.amount, amount_overdrawn_i64 );
            });

            break;
          }

          //if the amount requested is exactly the same as their overdrawn amount, erase the request and break
          else if( req_itr->wax_amount_requested.amount == amount_overdrawn_i64 ){

            //update the epoch so it properly tracks how much pending requests are tied to it
            epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
              _e.wax_to_refund.amount = safeSubInt64( _e.wax_to_refund.amount, amount_overdrawn_i64 );
            });

            req_itr = requests_t.erase( req_itr );

            break;
          }

          //if the amount requested is < the overdrawn amount, we need to check other epochs as well and 
          //continue debiting until they are not overdrawn. erase this request and continue the loop
          else{
            //calculate how much we still need to debit after this request is erased
            amount_overdrawn_i64 = safeSubInt64( amount_overdrawn_i64, req_itr->wax_amount_requested.amount );

            //update the epoch so it properly tracks how much pending requests are tied to it
            epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
              _e.wax_to_refund.amount = safeSubInt64( _e.wax_to_refund.amount, req_itr->wax_amount_requested.amount );
            });       

            req_itr = requests_t.erase( req_itr );     
          }

        } //end if their is a request
      } //end if the epoch exists 
    } //end for loop that checks epochs  
  } //end if the user is overdrawn

  return;
}

std::string fusion::cpu_stake_memo(const eosio::name& cpu_receiver, const uint64_t& epoch_timestamp){
  return ("|stake_cpu|" + cpu_receiver.to_string() + "|" + std::to_string(epoch_timestamp) + "|").c_str();
}

uint64_t fusion::days_to_seconds(const uint64_t& days){
  return (uint64_t) SECONDS_PER_DAY * days;
}

uint64_t fusion::get_seconds_to_rent_cpu( state s, config3 c, const uint64_t& epoch_id_to_rent_from ){
      uint64_t eleven_days = 60 * 60 * 24 * 11;
      uint64_t seconds_into_current_epoch = now() - s.last_epoch_start_time;

      uint64_t seconds_to_rent;

      if( epoch_id_to_rent_from == s.last_epoch_start_time + c.seconds_between_epochs ){
        //renting from epoch 3 (hasnt started yet)
        seconds_to_rent = days_to_seconds(18) - seconds_into_current_epoch;

        //see if this epoch exists yet - if it doesn't, create it
        auto next_epoch_itr = epochs_t.find( epoch_id_to_rent_from );

        if( next_epoch_itr == epochs_t.end() ){

          uint64_t next_epoch_start_time = s.last_epoch_start_time + c.seconds_between_epochs;

          int next_cpu_index = 1;
          bool contract_was_found = false;

          for(eosio::name cpu : c.cpu_contracts){

            if( cpu == s.current_cpu_contract ){
              contract_was_found = true;

              if(next_cpu_index == c.cpu_contracts.size()){
                next_cpu_index = 0;
              }
            }

            if(contract_was_found) break;
            next_cpu_index ++;
          }

          check( contract_was_found, "error locating cpu contract" );
          eosio::name next_cpu_contract = c.cpu_contracts[next_cpu_index];
          check( next_cpu_contract != s.current_cpu_contract, "next cpu contract can not be the same as the current contract" );

          //create the next epoch
          create_epoch( c, next_epoch_start_time, next_cpu_contract, ZERO_WAX );
        }

      } else if( epoch_id_to_rent_from == s.last_epoch_start_time ){
        //renting from epoch 2 (started most recently)
        seconds_to_rent = days_to_seconds(11) - seconds_into_current_epoch;

      } else if( epoch_id_to_rent_from == s.last_epoch_start_time - c.seconds_between_epochs ){
        //renting from epoch 1 (oldest) and we need to make sure it's less than 11 days old       
        check( seconds_into_current_epoch < days_to_seconds(4), "it is too late to rent from this epoch, please rent from the next one" );

        //if we reached here, minimum PAYMENT is 1 full day payment (even if rental is less than 1 day)
        seconds_to_rent = days_to_seconds(4) - seconds_into_current_epoch < days_to_seconds(1) ? days_to_seconds(1) : days_to_seconds(4) - seconds_into_current_epoch;

      } else{
        check( false, "you are trying to rent from an invalid epoch" );
      }

      return seconds_to_rent;
}


std::vector<std::string> fusion::get_words(std::string memo){
  std::string delim = "|";
  std::vector<std::string> words{};
  size_t pos = 0;
  while ((pos = memo.find(delim)) != std::string::npos) {
    words.push_back(memo.substr(0, pos));
    memo.erase(0, pos + delim.length());
  }
  return words;
}


/**
* is_an_admin
* this contract is multisig and all of the most important features will require msig
* there will be some features that do not pose any major risk to users/funds
* in which case, just having a few "admin" accounts who can call certain actions will be sufficient
* rather than needing to organize an msig with multiple parties over a minor adjustment
* e.g. - adjusting the cost for renting CPU or adjusting the fallback_cpu_receiver etc
*/

bool fusion::is_an_admin(const eosio::name& user){
  config3 c = config_s_3.get();

  if( std::find(c.admin_wallets.begin(), c.admin_wallets.end(), user) != c.admin_wallets.end() ){
    return true;
  }

  return false;
}

bool fusion::is_cpu_contract(const eosio::name& contract){
  config3 c = config_s_3.get();

  if( std::find( c.cpu_contracts.begin(), c.cpu_contracts.end(), contract) != c.cpu_contracts.end() ){
    return true;
  }

  return false;
}

void fusion::issue_lswax(const int64_t& amount, const eosio::name& receiver){
  action(permission_level{ _self, "active"_n }, TOKEN_CONTRACT, "issue"_n, std::tuple{ _self, receiver, asset(amount, LSWAX_SYMBOL), std::string("issuing lsWAX to liquify") }).send();
  return;
}

void fusion::issue_swax(const int64_t& amount){
  action(permission_level{ _self, "active"_n }, TOKEN_CONTRACT, "issue"_n, std::tuple{ _self, _self, asset(amount, SWAX_SYMBOL), std::string("issuing sWAX for staking") }).send();
  return;
}

bool fusion::memo_is_expected(const std::string& memo){
  if( memo == "instant redeem" || memo == "rebalance" || "wax_lswax_liquidity" || memo == "stake" || memo == "unliquify" || memo == "waxfusion_revenue" || memo == "cpu rental return" || memo == "lp_incentives" ){
    return true;
  }

  std::string memo_copy = memo;
  std::vector<std::string> words = get_words(memo_copy);

  if( words[1] == "rent_cpu" || words[1] == "unliquify_exact" ){
      return true;
  }  

  return false;
}

uint64_t fusion::now(){
  return current_time_point().sec_since_epoch();
}

void fusion::retire_lswax(const int64_t& amount){
  action(permission_level{get_self(), "active"_n}, TOKEN_CONTRACT,"retire"_n,std::tuple{ eosio::asset(amount, LSWAX_SYMBOL), std::string("retiring lsWAX to unliquify")}).send();
  return;
}

void fusion::retire_swax(const int64_t& amount){
  action(permission_level{get_self(), "active"_n}, TOKEN_CONTRACT,"retire"_n,std::tuple{ eosio::asset(amount, SWAX_SYMBOL), std::string("retiring sWAX for redemption")}).send();
  return;
}

void fusion::sync_epoch(state& s){
  //find out when the last epoch started
  config3 c = config_s_3.get();

  int next_cpu_index = 1;
  bool contract_was_found = false;

  for(eosio::name cpu : c.cpu_contracts){

    if( cpu == s.current_cpu_contract ){
      contract_was_found = true;

      if(next_cpu_index == c.cpu_contracts.size()){
        next_cpu_index = 0;
      }
    }

    if(contract_was_found) break;
    next_cpu_index ++;
  }

  check( contract_was_found, "error locating cpu contract" );
  eosio::name next_cpu_contract = c.cpu_contracts[next_cpu_index];
  check( next_cpu_contract != s.current_cpu_contract, "next cpu contract cant be the same as the current contract" );

  //calculate when the next is supposed to start
  uint64_t next_epoch_start_time = s.last_epoch_start_time + c.seconds_between_epochs;

  if( now() >= next_epoch_start_time ){

    s.last_epoch_start_time = next_epoch_start_time;
    s.current_cpu_contract = next_cpu_contract;

    auto epoch_itr = epochs_t.find(next_epoch_start_time);

    //this epoch should already exist due to CPU staking, but there's a possibility no CPU has been staked to it yet
    //if it doesn't exist, create it
    if(epoch_itr == epochs_t.end()){
      create_epoch( c, next_epoch_start_time, next_cpu_contract, ZERO_WAX );
    }
  }

  return;
}

/**
* sync_tvl
* updates the total value locked in the state2 singleton
* total_value_locked does not necessarily reflect the current state of the chain,
* but sync_tvl can be called often to periodically sync to the chain state
*/ 

void fusion::sync_tvl(){
  config3 c = config_s_3.get();
  state s = states.get();
  state2 s2 = state_s_2.get();
  state3 s3 = state_s_3.get();

  eosio::asset total_value_locked = ZERO_WAX;
  eosio::asset contract_wax_balance = ZERO_WAX;
  eosio::asset total_wax_owed = ZERO_WAX;

  //wax balance of this contract
  accounts account_t = accounts(WAX_CONTRACT, _self.value);
  const uint64_t raw_token_symbol = WAX_SYMBOL.code().raw();
  auto it = account_t.find(raw_token_symbol);
  if(it != account_t.end()){
    total_value_locked.amount = safeAddInt64( total_value_locked.amount, it->balance.amount );
    contract_wax_balance = it->balance;
  }

  //wax balance of pol.fusion
  accounts account_t_2 = accounts(WAX_CONTRACT, POL_CONTRACT.value);
  auto it_2 = account_t_2.find(raw_token_symbol);
  if(it_2 != account_t_2.end()){
    total_value_locked.amount = safeAddInt64( total_value_locked.amount, it_2->balance.amount );
  }  

  //total amount in both columns from pol_state_s_3
  pol_contract::state3 ps3 = pol_state_s_3.get();
  total_value_locked.amount = safeAddInt64( total_value_locked.amount, ps3.wax_allocated_to_rentals.amount );
  total_value_locked.amount = safeAddInt64( total_value_locked.amount, ps3.pending_refunds.amount );

  //total wax_bucket from the current 3 epochs
  uint64_t epoch_to_request_from = s.last_epoch_start_time - c.seconds_between_epochs;

  std::vector<uint64_t> epochs_to_check = {
    epoch_to_request_from,
    epoch_to_request_from + c.seconds_between_epochs,
    epoch_to_request_from + ( c.seconds_between_epochs * 2 )
  };

  for(uint64_t ep : epochs_to_check){
    auto epoch_itr = epochs_t.find(ep);

    if(epoch_itr != epochs_t.end()){
      total_value_locked.amount = safeAddInt64( total_value_locked.amount, epoch_itr->wax_bucket.amount );
    } 
  }   

  s2.total_value_locked = total_value_locked;
  state_s_2.set(s2, _self);

  total_wax_owed.amount = safeAddInt64( total_wax_owed.amount, s.wax_available_for_rentals.amount );
  total_wax_owed.amount = safeAddInt64( total_wax_owed.amount, s.revenue_awaiting_distribution.amount );
  total_wax_owed.amount = safeAddInt64( total_wax_owed.amount, s.wax_for_redemption.amount );
  total_wax_owed.amount = safeAddInt64( total_wax_owed.amount, s.user_funds_bucket.amount );
  total_wax_owed.amount = safeAddInt64( total_wax_owed.amount, s3.total_claimable_wax.amount );
  s3.total_wax_owed = total_wax_owed;
  s3.contract_wax_balance = contract_wax_balance;

  uint64_t last_key = 0;
  auto snap_it = state_snaps_t.end();

  if(state_snaps_t.begin() != state_snaps_t.end()){
      snap_it --;
      last_key = snap_it->timestamp;
  }  

  if( last_key + ( 60 * 30 ) < now() ){ /* if 30 mins have passed, create a snapshot of state */
    state_snaps_t.emplace(_self, [&](auto &_snap){
      _snap.timestamp = now();
      _snap.total_wax_owed = total_wax_owed;
      _snap.contract_wax_balance = contract_wax_balance;
    });
  }

  s3.last_update = now();
  state_s_3.set(s3, _self);

}

/** sync_user
 *  loops through any snapshots since their last update and calculates what they are owed
 *  in order to avoid re-entry attacks, but also prevent people from missing payouts during 
 *  edge cases such as syncing at the exact same timestamp as a payout, the following measures
 *  are in place:
 *  - if their last_update is now, return
 *  - if there are no payouts since their last update, return
 *  - if the payout timestamp is <= their last update, skip it
 *  - set their last update to now() if there is anything to process
 */

void fusion::sync_user(state& s, staker_struct& staker){
  config3 c = config_s_3.get(); 

  //only continue if they have swax staked
  if( staker.swax_balance.amount == 0 ){
    staker.last_update = now();
    return;
  }

  if(staker.last_update == now()) return;

  auto snaps_lower = snaps_t.lower_bound( staker.last_update );

  if(snaps_lower == snaps_t.end()) return;

  payouts_table payouts_t = payouts_table( _self, _self.value );

  int count = 0;
  int64_t wax_owed_to_user = 0; 
  int64_t wax_allocation;

  for(auto itr = snaps_lower; itr != snaps_t.end(); itr++){

    if( itr->timestamp <= s.next_distribution && count < c.max_snapshots_to_process && itr->timestamp > staker.last_update ){
      count ++;      
      //only calculate if there was sWAX earning
      if(itr->swax_earning_bucket.amount > 0){
        //calculate the % of snapshot owned by this user and add it to their owed_to
        wax_allocation = internal_get_wax_owed_to_user(staker.swax_balance.amount, itr->total_swax_earning.amount, itr->swax_earning_bucket.amount);
        wax_owed_to_user = safeAddInt64(wax_owed_to_user, wax_allocation);
    
      }
        //TODO REMOVE PAYOUTS TABLE
        payouts_t.emplace(_self, [&](auto &_row){
          _row.id = payouts_t.available_primary_key();
          _row.timestamp = count;
          _row.user = staker.wallet;
          _row.amount = asset(wax_allocation, WAX_SYMBOL);
        });          
    }

    if(count >= c.max_snapshots_to_process) break;

  }

  asset claimable_wax = staker.claimable_wax;
  claimable_wax.amount = safeAddInt64(claimable_wax.amount, wax_owed_to_user);

  //credit their balance, update last_update to now()
  staker.claimable_wax = claimable_wax;
  staker.last_update = now();

  credit_total_claimable_wax( eosio::asset(wax_owed_to_user, WAX_SYMBOL) );

  //debit the user bucket in state
  s.user_funds_bucket.amount = safeSubInt64(s.user_funds_bucket.amount, wax_owed_to_user);

  return;
}

void fusion::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo){
  action(permission_level{get_self(), "active"_n}, contract,"transfer"_n,std::tuple{ get_self(), user, amount_to_send, memo}).send();
  return;
}

void fusion::validate_distribution_amounts(const int64_t& user_alloc_i64, const int64_t& pol_alloc_i64, 
      const int64_t& eco_alloc_i64, const int64_t& swax_autocompounding_alloc_i64,
      const int64_t& swax_earning_alloc_i64, const int64_t& amount_to_distribute_i64)
{
  //check the total sum is in range
  int64_t alloc_check_1 = safeAddInt64(user_alloc_i64, pol_alloc_i64);
  int64_t alloc_check_2 = safeAddInt64(alloc_check_1, eco_alloc_i64);
  check( alloc_check_2 <= amount_to_distribute_i64, "allocation check 2 failed" );

  //check that the user sums are <= the user allocation
  int64_t alloc_check_3 = safeAddInt64(swax_autocompounding_alloc_i64, swax_earning_alloc_i64);
  check( alloc_check_3 <= user_alloc_i64, "allocation check 3 failed" );
  return;  
}

void fusion::validate_token(const eosio::symbol& symbol, const eosio::name& contract)
{
  if(symbol == WAX_SYMBOL){
    check(contract == WAX_CONTRACT, "expected eosio.token contract");
    return;
  }

  if(symbol == LSWAX_SYMBOL){
    check(contract == TOKEN_CONTRACT, "expected token.fusion contract");
    return;
  }  

  check(false, "invalid token received");
}


void fusion::zero_distribution(const state& s){
  create_snapshot(s, 0, 0, 0, 0, 0);  
  return; 
}