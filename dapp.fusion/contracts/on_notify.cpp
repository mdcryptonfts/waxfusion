#pragma once

void fusion::receive_token_transfer(name from, name to, eosio::asset quantity, std::string memo){
	const name tkcontract = get_first_receiver();

    if( quantity.amount == 0 ) return;

    if( from == get_self() || to != get_self() ){
    	return;
    }

    check( quantity.amount > 0, "must send a positive quantity" );
    check( quantity.amount < MAX_ASSET_AMOUNT, "quantity too large" );

    //accept random tokens but dont execute any logic
    if( !memo_is_expected( memo ) && tkcontract != WAX_CONTRACT && tkcontract != TOKEN_CONTRACT ) return;

    //only accept wax and lsWAX (sWAX is only issued, not transferred)
  	validate_token(quantity.symbol, tkcontract);

    //if we reached here, the token is either wax or lswax, but the memo is not expected
    if( !memo_is_expected( memo ) ){
    	//TODO either throw error or add to the bucket
    }

  	/** instant redeem and rebalance memos
  	 *  there are 2 cases where the POL contract will need to send LSWAX with these memos
  	 *  - if alcor price is out of range and it's been 1 week since last liquidity addition,
  	 * 		in which case the returned wax will go to the CPU rental pool
  	 *  - if alcor price is in range and POL needs to rebalance its assets before depositing
  	 *  	liquidity, in which case the returned funds will be used for liquidity instead
  	 */

  	if( memo == "instant redeem" || memo == "rebalance" ){

  		check( tkcontract == TOKEN_CONTRACT, "only LSWAX should be sent with this memo" );
  		
  		check( from == POL_CONTRACT, ( "expected " + POL_CONTRACT.to_string() + " to be the sender" ).c_str() );

	    state s = states.get();
  		config3 c = config_s_3.get();

  		sync_epoch( s );  		

  		check( quantity >= c.minimum_unliquify_amount, "minimum unliquify amount not met" );	    

	    int64_t swax_to_redeem = internal_unliquify( quantity.amount, s );

	    //make sure there is enough wax available
		check( s.wax_available_for_rentals.amount >= swax_to_redeem, "not enough instaredeem funds available" );

		//debit the amount from s.wax_available_for_rentals
		s.wax_available_for_rentals.amount = safeSubInt64( s.wax_available_for_rentals.amount, swax_to_redeem );

		//calculate the 0.05% share
		//calculate the remainder for the user
		uint64_t user_percentage_1e6 = ONE_HUNDRED_PERCENT_1E6 - PROTOCOL_FEE_1E6;
		int64_t protocol_share = calculate_asset_share( swax_to_redeem, PROTOCOL_FEE_1E6 );
		int64_t user_share = calculate_asset_share( swax_to_redeem, user_percentage_1e6 );

		check( safeAddInt64( protocol_share, user_share ) <= swax_to_redeem, "error calculating protocol fee" );

		//transfer the funds to pol contract
		std::string transfer_memo = memo == "instant redeem" ? "for staking pool only" : "rebalance";
		transfer_tokens( from, asset( user_share, WAX_SYMBOL ), WAX_CONTRACT, transfer_memo );

		//add the 0.05% to the revenue_awaiting_distribution
		s.revenue_awaiting_distribution.amount = safeAddInt64( s.revenue_awaiting_distribution.amount, protocol_share );

		//adjust the swax currently backing lswax
		s.swax_currently_backing_lswax.amount = safeSubInt64( s.swax_currently_backing_lswax.amount, swax_to_redeem );

		//adjust the liquified swax
		s.liquified_swax.amount = safeSubInt64( s.liquified_swax.amount, quantity.amount );		

		retire_swax(swax_to_redeem);	  
		retire_lswax(quantity.amount);  

  		states.set(s, _self);
  		return;
  	}

  	if( memo == "wax_lswax_liquidity" ){
  		check( tkcontract == WAX_CONTRACT, "only WAX should be sent with this memo" );
  		check( from == POL_CONTRACT, ( "expected " + POL_CONTRACT.to_string() + " to be the sender" ).c_str() );

  		state s = states.get();	

  		sync_epoch( s );  		
	    
		int64_t converted_lsWAX_i64 = internal_liquify(quantity.amount, s);

		s.swax_currently_backing_lswax.amount = safeAddInt64(s.swax_currently_backing_lswax.amount, quantity.amount);
		s.liquified_swax.amount = safeAddInt64(s.liquified_swax.amount, converted_lsWAX_i64);	  
		s.wax_available_for_rentals.amount = safeAddInt64(s.wax_available_for_rentals.amount, quantity.amount);    

	    states.set(s, _self);

	    issue_swax(quantity.amount);
		issue_lswax(converted_lsWAX_i64, _self);
		transfer_tokens( POL_CONTRACT, asset( converted_lsWAX_i64, LSWAX_SYMBOL ), TOKEN_CONTRACT, "liquidity" );

  		return;	    	
  	} 

  	/** stake memo
  	 *  used for creating new sWAX tokens at 1:1 ratio
  	 * 	these sWAX will be staked initially, (added to the wax_available_for_rentals balance)
  	 * 	they can be converted to liquid sWAX (lsWAX) afterwards
  	 */

  	if( memo == "stake" ){
  		check( tkcontract == WAX_CONTRACT, "only WAX is used for staking" );
  		
  		state s = states.get();
  		config3 c = config_s_3.get();

  		sync_epoch( s );

  		check( quantity >= c.minimum_stake_amount, "minimum stake amount not met" );
	
		auto staker_itr = staker_t.require_find(from.value, "you need to use the stake action first");
		staker_struct staker = { from, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
		sync_user( s, staker );

  		//add the deposit amount to their staked sWAX
  		eosio::asset staked_balance = staker.swax_balance;
  		staked_balance.amount = safeAddInt64(staked_balance.amount, quantity.amount);

  		staker_t.modify(staker_itr, same_payer, [&](auto &_s){
  			_s.swax_balance = staked_balance;
  			_s.last_update = staker.last_update;
  		});

  		//add this amount to the "currently_earning" sWAX bucket
  		//state should not be fetched until after epoch is synced
	    s.swax_currently_earning.amount = safeAddInt64(s.swax_currently_earning.amount, quantity.amount);
	    s.wax_available_for_rentals.amount = safeAddInt64(s.wax_available_for_rentals.amount, quantity.amount);
	    states.set(s, _self);

  		//issue new sWAX to dapp contract
  		issue_swax(quantity.amount);   	    

  		return;
  	}

  	/** unliquify memo
  	 *  used for converting lsWAX back to sWAX
  	 * 	rate is not 1:1, needs to be fetched from state table
  	 */

  	if( memo == "unliquify" ){
  		//front end has to package in a "stake" action before transferring, to make sure they have a row
  		state s = states.get();
  		config3 c = config_s_3.get();

  		sync_epoch( s );

  		check( tkcontract == TOKEN_CONTRACT, "only LSWAX can be unliquified" );
  		check( quantity >= c.minimum_unliquify_amount, "minimum unliquify amount not met" );

  		int64_t converted_sWAX_i64 = internal_unliquify(quantity.amount, s);

  		//debit the amount from liquified sWAX
  		s.liquified_swax.amount = safeSubInt64(s.liquified_swax.amount, quantity.amount);
  		s.swax_currently_backing_lswax.amount = safeSubInt64(s.swax_currently_backing_lswax.amount, converted_sWAX_i64);

  		//add this amount to the "currently_earning" sWAX bucket
  		s.swax_currently_earning.amount = safeAddInt64(s.swax_currently_earning.amount, converted_sWAX_i64); 

		auto staker_itr = staker_t.require_find(from.value, "you need to use the stake action first");
		staker_struct staker = { from, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
		sync_user( s, staker );

  		eosio::asset staked_balance = staker.swax_balance;
  		staked_balance.amount = safeAddInt64(staked_balance.amount, converted_sWAX_i64);

  		staker_t.modify(staker_itr, same_payer, [&](auto &_s){
  			_s.swax_balance = staked_balance;
  			_s.last_update = staker.last_update;
  		});

  		states.set(s, _self);

  		//retire the lsWAX
  		retire_lswax(quantity.amount);  		

  		return;

  	}

  	/** waxfusion_revenue
  	 * 	used for receiving revenue from helper contracts, like CPU rentals, wax staking, etc
  	 */

  	if( memo == "waxfusion_revenue" ){
  		check( tkcontract == WAX_CONTRACT, "only WAX is accepted with waxfusion_revenue memo" );

  		//add the wax to state.revenue_awaiting_distribution
  		state s = states.get();
  		s.revenue_awaiting_distribution.amount = safeAddInt64(s.revenue_awaiting_distribution.amount, quantity.amount);
  		states.set(s, _self); 
  		return;
  	}

  	/** lp_incentives
  	 *  to be used as rewards for creating farms on alcor
  	 */ 

  	if( memo == "lp_incentives" ){
  		check( tkcontract == WAX_CONTRACT, "only WAX is accepted with lp_incentives memo" );

  		state s = states.get();
  		state2 s2 = state_s_2.get();

  		sync_epoch( s );

  		//calculate how much LSWAX to issue
		int64_t converted_lsWAX_i64 = internal_liquify( quantity.amount, s );		

		//issue swax and lswax
		issue_swax(quantity.amount);
		issue_lswax(converted_lsWAX_i64, _self);

		//update the state
  		s2.incentives_bucket.amount = safeAddInt64( s2.incentives_bucket.amount, converted_lsWAX_i64 );
  		s.wax_available_for_rentals.amount = safeAddInt64( s.wax_available_for_rentals.amount, quantity.amount );
  		s.swax_currently_backing_lswax.amount = safeAddInt64( s.swax_currently_backing_lswax.amount, quantity.amount );
  		s.liquified_swax.amount = safeAddInt64(s.liquified_swax.amount, converted_lsWAX_i64);

  		states.set(s, _self);
  		state_s_2.set(s2, _self);

  		return;
  	}

  	/** cpu rental return
  	 * 	these are funds coming back from one of the CPU rental contracts after being staked/rented
  	 */

  	if( memo == "cpu rental return" ){
  		check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
  		check( is_cpu_contract(from), "sender is not a valid cpu rental contract" );

  		state s = states.get();
  		config3 c = config_s_3.get();

  		sync_epoch( s );

  		/** 
  		* this SHOULD always belong to last epoch - 2 epochs
  		* i.e. by the time this arrives from epoch 1,
  		* it should now be epoch 3
  		*/

  		uint64_t relevant_epoch = s.last_epoch_start_time - c.cpu_rental_epoch_length_seconds;

  		//look up this epoch
  		auto epoch_itr = epochs_t.require_find(relevant_epoch, "could not locate relevant epoch");

  		while( epoch_itr->cpu_wallet != from ){
  			epoch_itr --;
  		}

  		//make sure the cpu contract is a match
  		check( epoch_itr->cpu_wallet == from, "sender does not match wallet linked to epoch" );

  		//add the relevant amount to the redemption bucket
  		asset total_added_to_redemption_bucket = epoch_itr->total_added_to_redemption_bucket;
  		asset amount_to_send_to_rental_bucket = quantity;

  		if( epoch_itr->total_added_to_redemption_bucket < epoch_itr->wax_to_refund ){
  			//there are still funds to add to redemption
  			int64_t amount_added = 0;

  			int64_t amount_remaining_to_add = safeSubInt64(epoch_itr->wax_to_refund.amount, epoch_itr->total_added_to_redemption_bucket.amount);

  			if(amount_remaining_to_add >= quantity.amount){
  				//the whole quantity will go to the redemption bucket
  				amount_added = quantity.amount;
  				
  			} else {
  				//only a portion of the amount will go to the redemption bucket
  				amount_added = amount_remaining_to_add;
  			}

  			total_added_to_redemption_bucket.amount = safeAddInt64(total_added_to_redemption_bucket.amount, amount_added);
  			amount_to_send_to_rental_bucket.amount = safeSubInt64(amount_to_send_to_rental_bucket.amount, amount_added);
  			s.wax_for_redemption.amount = safeAddInt64(s.wax_for_redemption.amount, amount_added);
  		}

  		//add the remainder to available for rental bucket
  		if(amount_to_send_to_rental_bucket.amount > 0){
  			s.wax_available_for_rentals.amount = safeAddInt64(s.wax_available_for_rentals.amount, amount_to_send_to_rental_bucket.amount);
  		}

  		//update cpu funds returned in epoch table
  		asset total_cpu_funds_returned = epoch_itr->total_cpu_funds_returned;
  		total_cpu_funds_returned.amount = safeAddInt64(total_cpu_funds_returned.amount, quantity.amount);

  		epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
  			_e.total_cpu_funds_returned = total_cpu_funds_returned;
  			_e.total_added_to_redemption_bucket = total_added_to_redemption_bucket;
  		});

  		states.set(s, _self);

  		return;
  	}

  	/**
  	* @get_words
  	* anything below here should be a dynamic memo with multiple words to parse
  	*/

  	std::vector<std::string> words = get_words( memo );

  	/**
  	* rent_cpu
  	* if it has this, we need to parse it and find out which epoch, and who to rent to
  	*/ 

  	if( words[1] == "rent_cpu" ){
  		check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
  		check( words.size() >= 5, "memo for rent_cpu operation is incomplete" );

  		//memo should also include account to rent to 
  		const eosio::name cpu_receiver = eosio::name( words[2] );
  		check( is_account( cpu_receiver ), ( cpu_receiver.to_string() + " is not an account" ).c_str() );  

  		//memo should also include amount of wax to rent
  		//this is a "whole" number of wax. '10' == 10.00000000 WAX
  		//so it needs to be scaled by 1e8
  		const uint64_t wax_amount_to_rent = std::strtoull( words[3].c_str(), NULL, 0 );

  		//memo should include an epoch ID
  		const uint64_t epoch_id_to_rent_from = std::strtoull( words[4].c_str(), NULL, 0 );
  		const uint64_t amount_to_rent_with_precision = safeMulUInt64( wax_amount_to_rent, SCALE_FACTOR_1E8 );  		

  		//that amount should be > min_rental
  		check( wax_amount_to_rent >= MINIMUM_WAX_TO_RENT, ( "minimum wax amount to rent is " + std::to_string( MINIMUM_WAX_TO_RENT ) ).c_str() );
  		check( wax_amount_to_rent <= MAXIMUM_WAX_TO_RENT, ( "maximum wax amount to rent is " + std::to_string( MAXIMUM_WAX_TO_RENT ) ).c_str() );  				

  		state s = states.get();
  		config3 c = config_s_3.get();

  		sync_epoch( s );

  		//make sure there is anough wax available for this rental
  		check( s.wax_available_for_rentals.amount >= amount_to_rent_with_precision, "there is not enough wax in the rental pool to cover this rental" );

  		//debit the wax from the rental pool
  		s.wax_available_for_rentals.amount = safeSubInt64(s.wax_available_for_rentals.amount, amount_to_rent_with_precision);

  		uint64_t seconds_to_rent = get_seconds_to_rent_cpu(s, c, epoch_id_to_rent_from);

  		int64_t expected_amount_received = s.cost_to_rent_1_wax.amount * wax_amount_to_rent * seconds_to_rent / days_to_seconds(1);

  		check( quantity.amount >= expected_amount_received, ( "expected to receive " + eosio::asset( expected_amount_received, WAX_SYMBOL ).to_string() ).c_str() );
  		s.revenue_awaiting_distribution.amount = safeAddInt64( s.revenue_awaiting_distribution.amount, expected_amount_received );

  		//if they sent more than expected, calculate difference and refund it
  		if( quantity.amount > expected_amount_received ){
  			//calculate difference
  			int64_t amount_to_refund = safeSubInt64(quantity.amount, expected_amount_received);

  			if( amount_to_refund > 0 ){
  				transfer_tokens( from, asset( amount_to_refund, WAX_SYMBOL ), WAX_CONTRACT, "cpu rental refund from waxfusion.io - liquid staking protocol" );
  			}
  		}

  		auto epoch_itr = epochs_t.require_find( epoch_id_to_rent_from, ("epoch " + std::to_string(epoch_id_to_rent_from) + " does not exist").c_str() );

  		//send funds to the cpu contract
  		transfer_tokens( epoch_itr->cpu_wallet, asset( (int64_t) amount_to_rent_with_precision, WAX_SYMBOL), WAX_CONTRACT, cpu_stake_memo(cpu_receiver, epoch_id_to_rent_from) );

  		//update the epoch's wax_bucket
  		asset epoch_wax_bucket = epoch_itr->wax_bucket;
  		epoch_wax_bucket.amount = safeAddInt64(epoch_wax_bucket.amount, (int64_t) amount_to_rent_with_precision);

  		epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
  			_e.wax_bucket = epoch_wax_bucket;
  		});

  		//add this rental into the renters table
  		renters_table renters_t = renters_table( _self, epoch_id_to_rent_from );
        auto renter_receiver_idx = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo = mix64to128(from.value, cpu_receiver.value);

        auto rental_itr = renter_receiver_idx.find(renter_receiver_combo);

        if( rental_itr == renter_receiver_idx.end() ){
        	renters_t.emplace(_self, [&](auto &_r){
        		_r.ID = renters_t.available_primary_key();
        		_r.renter = from;
        		_r.rent_to_account = cpu_receiver;
        		_r.amount_staked = eosio::asset( (int64_t) amount_to_rent_with_precision, WAX_SYMBOL );
        	});
        } else {
        	renter_receiver_idx.modify(rental_itr, _self, [&](auto &_r){
        		_r.amount_staked.amount = safeAddInt64( _r.amount_staked.amount, (int64_t) amount_to_rent_with_precision );
        	});
        }

  		//update the state
  		states.set(s, _self);

  		return;
  	}

  	if( words[1] == "unliquify_exact" ){

  		check( words.size() >= 4, "memo for unliquify_exact operation is incomplete" );
  		check( tkcontract == TOKEN_CONTRACT, "only LSWAX can be unliquified" );

  		config3 c = config_s_3.get();
  		state s = states.get();

  		sync_epoch( s );

  		check( quantity >= c.minimum_unliquify_amount, "minimum unliquify amount not met" );

  		const uint64_t expected_output = std::strtoull( words[2].c_str(), NULL, 0 );
  		const uint64_t max_slippage = std::strtoull( words[3].c_str(), NULL, 0 );

  		//calculate the conversion rate (amount of sWAX to stake to this user)
  		int64_t converted_sWAX_i64 = internal_unliquify(quantity.amount, s);

		check( max_slippage >= 0 && max_slippage < ONE_HUNDRED_PERCENT_1E6, "max slippage is out of range" );
		check( expected_output > (uint64_t) 0 && expected_output <= MAX_ASSET_AMOUNT_U64, "expected output is out of range" );

		uint64_t minimum_output_percentage = ONE_HUNDRED_PERCENT_1E6 - max_slippage;
		int64_t minimum_output = calculate_asset_share( expected_output, minimum_output_percentage );

		check( converted_sWAX_i64 >= minimum_output, "output would be " + asset(converted_sWAX_i64, SWAX_SYMBOL).to_string() + " but expected " + asset(minimum_output, SWAX_SYMBOL).to_string() );	  		

  		//debit the amount from liquified sWAX
  		s.liquified_swax.amount = safeSubInt64(s.liquified_swax.amount, quantity.amount);
  		s.swax_currently_backing_lswax.amount = safeSubInt64(s.swax_currently_backing_lswax.amount, converted_sWAX_i64);

  		//add this amount to the "currently_earning" sWAX bucket
  		s.swax_currently_earning.amount = safeAddInt64(s.swax_currently_earning.amount, converted_sWAX_i64);  

		auto staker_itr = staker_t.require_find(from.value, "you need to use the stake action first");
		staker_struct staker = { from, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
		sync_user( s, staker );

  		eosio::asset staked_balance = staker.swax_balance;
  		staked_balance.amount = safeAddInt64(staked_balance.amount, converted_sWAX_i64);

  		staker_t.modify(staker_itr, same_payer, [&](auto &_s){
  			_s.swax_balance = staked_balance;
  			_s.last_update = staker.last_update;
  		});

  		states.set(s, _self); 

  		retire_lswax(quantity.amount);

  		return;
  	}  	

}