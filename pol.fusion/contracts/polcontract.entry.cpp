#pragma once

#include "polcontract.hpp"
#include "functions.cpp"
#include "integer_functions.cpp"
#include "on_notify.cpp"
#include "safe.cpp"
#include "scaling.cpp"

//contractName: polcontract

/** claimgbmvote
 *  if this contract has any voting rewards available on the system contract,
 *  this will claim them so the rewards can be distributed to the dapp.fusion contract
 *  (see: "voter pay" memo in on_notify.cpp)
 */

ACTION polcontract::claimgbmvote()
{
	//Can be called by anyone
	update_state();
	action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"claimgbmvote"_n,std::tuple{ get_self() }).send();
}

/** claimrefund
 *  if this contract has any available refunds on the system contract (from unstaking CPU)
 *  this will claim them so the funds become available again for renting
 */

ACTION polcontract::claimrefund()
{
	//Can be called by anyone
	update_state();
	action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"refund"_n,std::tuple{ get_self() }).send();
}

/**
* clearexpired
* unstakes and removes all expired rentals
* can be called by anyone
*/ 

ACTION polcontract::clearexpired(const int& limit)
{
	update_state();
	state3 s = state_s_3.get();

	//see if there is a pending refund for us in the refunds table
	auto refund_itr = refunds_t.find( get_self().value );

	if(refund_itr != refunds_t.end()){
		bool shouldContinue = false;

		/** the refund is less than 5 mins old, proceed
		 *  the reason for this is that once we have refunds pending for any significant
		 *  amount of time, we don't want to reset the timer on the refund request
		 * 	by submitting a new unstake request.
		 *  however, we may need to submit multiple unstake requests in a short period
		 *  of time (for example if there are 100k accounts that we are renting to)
		 *  this allows us to break up unstake requests into multiple batches if needed
		 */
		if( now() - (5 * 60) < uint64_t(refund_itr->request_time.sec_since_epoch()) ){
			shouldContinue = true;
		}

		/* the refund is > 3 days old, claim it and proceed */
		else if( now() - days_to_seconds(3) > uint64_t(refund_itr->request_time.sec_since_epoch()) ){
			action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"refund"_n,std::tuple{ get_self() }).send();
			shouldContinue = true;
		}

		check( shouldContinue, "there is a pending refund > 5 mins and < 3 days old" );
	}

	//get expires index from renters table
	auto expires_idx = renters_t.get_index<"expires"_n>();

	//upper bound is now, lower bound is 1 
	//to avoid paying for deletion of unfunded rentals, which are set to expire at 0 by default
	auto expires_lower = expires_idx.lower_bound( 1 );
	auto expires_upper = expires_idx.upper_bound( now() );

	int count = 0;

    auto itr = expires_lower;
    while (itr != expires_upper) {
    	if( count == limit ) break;
		if( itr->expires < now() && itr->expires != 0 ){
			//this wax is no longer being allocated to a rental since we are about to unstake it
			s.wax_allocated_to_rentals.amount = safeSubInt64( s.wax_allocated_to_rentals.amount, itr->amount_staked.amount );
			
			//update the pending refunds bucket so we can keep track of how much is coming in
			s.pending_refunds.amount = safeAddInt64( s.pending_refunds.amount, itr->amount_staked.amount );

			//undelegatebw from the receiver
			action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"undelegatebw"_n,std::tuple{ get_self(), itr->rent_to_account, ZERO_WAX, itr->amount_staked}).send();
			itr = expires_idx.erase( itr );
			count++;
		} else {
            itr ++;
        }
    }

    check( count > 0, "no expired rentals to clear" );
    state_s_3.set(s, _self);
}

/** deleterental
 *  in case a user uses the rentcpu action and doesnt pay for the rental
 *  this will allow them to delete it and free up their ram
 */
ACTION polcontract::deleterental(const uint64_t& rental_id){
	auto itr = renters_t.require_find( rental_id, "rental does not exist" );
	require_auth( itr->renter );
	check( itr->expires == 0, "can not delete a rental after funding it, use the clearexpired action" );
	itr = renters_t.erase( itr );
	return;
}

ACTION polcontract::initconfig(){
	require_auth( _self );

	eosio::check( !config_s_2.exists(), "config2 already exists" );

	uint64_t rental_pool_allocation_1e6 = 14285714; //14.28% or 1/7th
	uint64_t liquidity_allocation_1e6 = ONE_HUNDRED_PERCENT_1E6 - rental_pool_allocation_1e6;

	config2 c{};
	c.liquidity_allocation_1e6 = liquidity_allocation_1e6;
	c.rental_pool_allocation_1e6 = rental_pool_allocation_1e6;
	c.lswax_wax_pool_id = TESTNET ? 2 : 9999999; //TODO change this when pair is created on mainnet
	config_s_2.set(c, _self);
}


ACTION polcontract::initstate3(){
	require_auth( _self );
	eosio::check(!state_s_3.exists(), "state3 already exists");

	state3 s{};
	s.wax_available_for_rentals = ZERO_WAX;
	s.next_day_end_time = now() + SECONDS_PER_DAY;
	s.cost_to_rent_1_wax = asset(120000, WAX_SYMBOL); // 0.0012 WAX per day
	s.last_vote_time = 0;
	s.wax_bucket = ZERO_WAX;
	s.lswax_bucket = ZERO_LSWAX;
	s.last_liquidity_addition_time = now();
	s.wax_allocated_to_rentals = ZERO_WAX;
	s.pending_refunds = ZERO_WAX;
	s.last_rebalance_time = 0;
	state_s_3.set(s, _self);
}

/**
* rebalance
* can be called by anyone
* if assets in the wax/lswax buckets are not
* proportional based on the real price (according to the dapp.fusion contract),
* the assets will be rebalanced accordingly
* if its been >= 1 week since the last time alcor's price was acceptable, this will
* also move stale funds from the buckets into the rental pool so people can rent wax
*/

ACTION polcontract::rebalance(){
	update_state();
	state3 s = state_s_3.get();
    config2 c = config_s_2.get();
    dapp_tables::config3 dc3 = dapp_config_s.get();	
    dapp_tables::state ds = dapp_state_s.get();	

    if( s.wax_bucket == ZERO_WAX && s.lswax_bucket == ZERO_LSWAX ){
    	check(false, "there are no assets to rebalance");
    }

    //since there is a 0.05% instant redemption fee for selling lswax to the protocol,
    //only allow rebalancing once per 24h to avoid any potential security issues
    //that could cause the protocol to lose funds
    check( s.last_rebalance_time <= now() - SECONDS_PER_DAY, "can only rebalance once every 24 hours" );

    liquidity_struct lp_details = get_liquidity_info( c, ds );

    if( !lp_details.is_in_range && s.last_liquidity_addition_time < now() - ( days_to_seconds(7) ) ){
    	//its been 1 week and alcor is still out of range
    	//we can move wax into the rental pool
    	//if there are any lswax, we can also check the instant redemption pool
    	//and redeem the max possible with our lswax
    	//then in the wax transfer, the dapp contract can put "for staking pool only" as memo
    	//this way the converted lswax -> wax is also available for CPU rental

    	bool can_rebalance = false;

    	if( s.wax_bucket > ZERO_WAX ){
    		s.wax_available_for_rentals.amount = safeAddInt64( s.wax_available_for_rentals.amount, s.wax_bucket.amount );
    		s.wax_bucket = ZERO_WAX;
    		can_rebalance = true;
    	}

    	//only attempt unliquifying if we have enough lswax to meet the minimum
    	//and make sure we have a positive lswax balance
    	//make sure ds.wax_available_for_rentals.amount is positive before unsecurely calling internal_liquify
    	if( s.lswax_bucket.amount > 0 && s.lswax_bucket >= dc3.minimum_unliquify_amount && ds.wax_available_for_rentals.amount > 0 ){
    		//find out the max lswax that can be redeemed
    		int64_t max_redeemable = internal_liquify( ds.wax_available_for_rentals.amount, ds );

    		//make sure at least one of the conditions is met before updating
    		//the state (either wax bucket wax > ZERO_WAX, or we unliquified)
    		//the can_rebalance flag accomplishes this

    		if( max_redeemable > dc3.minimum_unliquify_amount.amount ){
    			can_rebalance = true;

	    		//amount to transfer is min( lswax_bucket, dapp.available_for_rentals )
	    		int64_t amount_to_transfer = std::min( s.lswax_bucket.amount, max_redeemable );

	    		//extra check to make sure we are transfering a positive quantity
	    		check( amount_to_transfer > 0, "can not transfer this amount" );

	    		//sub the amount from lswax bucket
	    		s.lswax_bucket.amount = safeSubInt64( s.lswax_bucket.amount, amount_to_transfer );

	    		//make the transfer 
	    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("instant redeem") );

	    	}

    	}

    	check( can_rebalance, "can not rebalance, either due to empty buckets or dapp contract not having instant redemption funds" );

		s.last_rebalance_time = now();
		state_s_3.set(s, _self);
		return;

    } else if( lp_details.is_in_range ){
    	//alcor's price is acceptable

    	if( s.wax_bucket > ZERO_WAX && s.lswax_bucket == ZERO_LSWAX ){
    		//we need to convert wax into lswax

    		//all we need to do is convert half of the wax to lswax
    		//since we are buying at real_price
    		//and not adding liquidity when alcor price diverges too much

    		//calculate_asset_share takes in a 1e6 scaled % (50% == 50,000,000)
    		int64_t amount_to_transfer = calculate_asset_share( s.wax_bucket.amount, 50000000 );

    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );

    		s.wax_bucket.amount = safeSubInt64( s.wax_bucket.amount, amount_to_transfer );
    		s.last_rebalance_time = now();
			state_s_3.set(s, _self);
			return;	

    	} else if( s.lswax_bucket > ZERO_LSWAX && s.wax_bucket == ZERO_WAX ){
    		//we need to convert lswax into wax

    		//check that the lswax bucket has enough to meet the minimum unliquify amount
    		//and validate the ds.wax_available_for_rentals.amount before calling internal_liquify
    		//(internal_liq/unliq should never be called without first checking the quantity is positive)
	    	if( s.lswax_bucket >= dc3.minimum_unliquify_amount && ds.wax_available_for_rentals.amount > 0 ){		

	    		//find out the max lswax that can be redeemed
	    		int64_t max_redeemable = internal_liquify( ds.wax_available_for_rentals.amount, ds );

	    		check( max_redeemable > dc3.minimum_unliquify_amount.amount, 
	    			( DAPP_CONTRACT.to_string() + " doesn't have enough wax in the instant redemption pool to rebalance " )
	    			.c_str() );	 	    		

	    		//calculate the amount of swax our lswax is worth
	    		int64_t max_output_amount = s.lswax_bucket.amount > 0 ? internal_unliquify( s.lswax_bucket.amount, ds ) : 0;

	    		//amount to transfer is min( lswax_bucket, dapp.available_for_rentals )
	    		int64_t max_weight = std::min( max_output_amount, max_redeemable );

	    		//before we divide by 2, make sure the result would be positive
	    		check( max_weight > 1, "division would result in a nonpositive quantity" );

	    		//the weight of our transfer before factoring in the real price (by calling internal_liquify)
	    		int64_t weighted_amount_to_transfer = safeDivInt64(max_weight, 2);

	    		//calculate how much lswax is needed to represent the underlying wax amount
	    		//we can assume weighted_amount_to_transfer is positive due to the previous checks
	    		int64_t amount_to_transfer = internal_liquify( weighted_amount_to_transfer, ds );

	    		//extra check to make sure we are transfering a positive quantity
	    		check( amount_to_transfer > 0, "can not transfer this amount" );	    		

	    		//sub the amount from lswax bucket
	    		s.lswax_bucket.amount = safeSubInt64( s.lswax_bucket.amount, amount_to_transfer );

	    		//make the transfer 
	    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("rebalance") );
	    	} else {
	    		//we have lswax but its not possible to rebalance because the dapp doesnt have funds, or the lswax amount is too small
	    		check( false, "can not rebalance because we're unable to instant redeem lswax for wax" );
	    	}

	    	s.last_rebalance_time = now();
			state_s_3.set(s, _self);
			return;	    	    

    	} else {
    		//we already checked to make sure both buckets were not empty in this action
    		//so if we reach this block after checking the individual buckets,
    		//we can assume that both buckets have a balance

    		//get the weight of the lswax bucket (amount of wax that its worth according to dapp contract)
    		int64_t weighted_lswax_bucket = internal_unliquify( s.lswax_bucket.amount, ds );

    		//wax bucket is already weighted at 1, so just add it to lswax weight to get total weight
    		int64_t total_weight = safeAddInt64( weighted_lswax_bucket, s.wax_bucket.amount );

    		//calculate half of the total weight, so we can compare it against each bucket
    		//in order to determine if we need to rebalance (and how much)
    		int64_t half_weight = safeDivInt64(total_weight, 2);

    		if( half_weight > weighted_lswax_bucket ){
    			//we have to buy some lswax

    			//find out the difference between the desired weight and the current weight
    			//since wax is already 1:1 weight, there is no need for adjusting
    			int64_t amount_to_transfer = safeSubInt64( half_weight, weighted_lswax_bucket );

    			//dont bother rebalancing if the amount < 5 WAX threshold
    			if( amount_to_transfer >= 500000000 ){
		    		//sub the lesser amount from the lswax bucket
		    		s.wax_bucket.amount = safeSubInt64( s.wax_bucket.amount, amount_to_transfer );

		    		//make the transfer 
		    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );

		    		//the dapp contract will return the lswax after minting and liquifying our swax
		    		//the returned lswax will be handled in a notification under "liquidity" memo
    			} else {
    				check( false, "amount to rebalance is too small" );
    			}

    			s.last_rebalance_time = now();
				state_s_3.set(s, _self);
				return;	    							

    		} else if( half_weight > s.wax_bucket.amount ){
    			//we have to instant redeem some lswax

    			//find out the difference between the desired weight and the current weight
    			int64_t weight_difference = safeSubInt64( half_weight, s.wax_bucket.amount );

    			//figure out how many lswax == the weight difference
    			//can assume weight_difference is positive since we checked half_weight > wax_bucket
    			int64_t difference_adjusted = internal_liquify( weight_difference, ds );

    			//find out how much lswax the dapp contract has available for instant redemptions
    			int64_t max_redeemable = 
    				ds.wax_available_for_rentals.amount > 0 ?
    					internal_liquify( ds.wax_available_for_rentals.amount, ds )
    				: 0;

    			//the amount we want to transfer is the lesser of the 2 previous calculations
    			//either the max that the dapp contract can instant redeem,
    			//or the max amount we need to rebalance (difference_adjusted)
    			int64_t amount_to_transfer = std::min( difference_adjusted, max_redeemable );

    			//dont bother rebalancing if the amount < 5 LSWAX threshold
    			if( amount_to_transfer >= 500000000 ){
		    		//sub the lesser amount from the lswax bucket
		    		s.lswax_bucket.amount = safeSubInt64( s.lswax_bucket.amount, amount_to_transfer );

		    		//make the transfer 
		    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("rebalance") );

		    		//the dapp contract will return the wax after unliquifying our lswax
		    		//the returned wax will be handled in a notification under "rebalance" memo
    			} else {
    				check( false, "amount to rebalance is too small" );
    			}

    			s.last_rebalance_time = now();
				state_s_3.set(s, _self);
				return;	    			
    		}
    	}

    } else {
    	//alcor is not in range, and it hasnt been a week
    	//there is no point in rebalancing
    	check( false, "no need to rebalance" );	
    }
}

/**
* rentcpu
* to announce a rental and create a table row if it doesnt exist yet
* user is the ram payer on the row
*/

ACTION polcontract::rentcpu(const eosio::name& renter, const eosio::name& cpu_receiver){
	require_auth(renter);
	update_state();

	check( is_account(cpu_receiver), (cpu_receiver.to_string() + " is not a valid account").c_str() );

	auto renter_receiver_idx = renters_t.get_index<"fromtocombo"_n>();
	const uint128_t renter_receiver_combo = mix64to128(renter.value, cpu_receiver.value);

	auto itr = renter_receiver_idx.find(renter_receiver_combo);	

	if(itr == renter_receiver_idx.end()){
		renters_t.emplace(renter, [&](auto &_r){
			_r.ID = renters_t.available_primary_key();
			_r.renter = renter;
			_r.rent_to_account = cpu_receiver;
			_r.amount_staked = ZERO_WAX;
			_r.expires = 0; 		
		});
	}
}

/** setallocs
 *  allows this contract to adjust the allocation of revenue that is used for liquidity
 *  the only other allocation is the rental pool allocation,
 *  so calling this action will also set the rental pool allocation at
 * 	100% - liquidity allocation
 */

ACTION polcontract::setallocs(const uint64_t& liquidity_allocation_percent_1e6){
	require_auth( _self );
	eosio::check(config_s_2.exists(), "config2 doesnt exist");

	//allow 1-100%
	check( liquidity_allocation_percent_1e6 >= 1000000 && liquidity_allocation_percent_1e6 <= ONE_HUNDRED_PERCENT_1E6, "percent must be between > 1e6 && <= 100 * 1e6" );


	config2 c = config_s_2.get();
	c.liquidity_allocation_1e6 = liquidity_allocation_percent_1e6;
	c.rental_pool_allocation_1e6 = ONE_HUNDRED_PERCENT_1E6 - liquidity_allocation_percent_1e6;
	config_s_2.set(c, _self);
}

/** setrentprice
 *  this is called inline on the dapp.fusion contract
 *  it adjusts the cost that we charge for CPU rentals
 *  cost_to_rent_1_wax = the price of renting 1.00000000 WAX for 24 hours
 */

ACTION polcontract::setrentprice(const eosio::asset& cost_to_rent_1_wax){
	require_auth( DAPP_CONTRACT );
	update_state();
	check( cost_to_rent_1_wax.amount > 0, "cost must be positive" );
	check( cost_to_rent_1_wax.symbol == WAX_SYMBOL, "symbol and precision must match WAX" );

	state3 s = state_s_3.get();
	s.cost_to_rent_1_wax = cost_to_rent_1_wax;
	state_s_3.set(s, _self);
}