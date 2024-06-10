#pragma once

#include "polcontract.hpp"
#include "functions.cpp"
#include "integer_functions.cpp"
#include "on_notify.cpp"
#include "safe.cpp"

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

		else if( now() - days_to_seconds(3) > uint64_t(refund_itr->request_time.sec_since_epoch()) ){
			action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"refund"_n,std::tuple{ get_self() }).send();
			shouldContinue = true;
		}

		check( shouldContinue, "there is a pending refund > 5 mins and < 3 days old" );
	}

	auto expires_idx = renters_t.get_index<"expires"_n>();

	//lower bound of 1 because we don't want to pay for deletion of unpaid rentals,
	//which are set to expire at 0 by default
	auto expires_lower = expires_idx.lower_bound( 1 );
	auto expires_upper = expires_idx.upper_bound( now() );

	int count = 0;

    auto itr = expires_lower;
    while (itr != expires_upper) {
    	if( count == limit ) break;
		if( itr->expires < now() && itr->expires != 0 ){

			s.wax_allocated_to_rentals -= itr->amount_staked;
			s.pending_refunds += itr->amount_staked;

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
	renters_t.erase( itr );
}

ACTION polcontract::initconfig(const uint64_t& lswax_pool_id){
	require_auth( _self );

	check( !config_s_2.exists(), "config2 already exists" );

	auto itr = pools_t.require_find( lswax_pool_id, "pool does not exist on alcor" );
	validate_liquidity_pair( itr->tokenA, itr->tokenB );

	uint64_t rental_pool_allocation_1e6 = 14285714; //14.28% or 1/7th
	uint64_t liquidity_allocation_1e6 = ONE_HUNDRED_PERCENT_1E6 - rental_pool_allocation_1e6;

	config2 c{};
	c.liquidity_allocation_1e6 = liquidity_allocation_1e6;
	c.rental_pool_allocation_1e6 = rental_pool_allocation_1e6;
	c.lswax_wax_pool_id = lswax_pool_id;
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
    dapp_tables::global ds = dapp_state_s.get();	

    if( s.wax_bucket == ZERO_WAX && s.lswax_bucket == ZERO_LSWAX ){
    	check(false, "there are no assets to rebalance");
    }

    check( s.last_rebalance_time <= now() - SECONDS_PER_DAY, "can only rebalance once every 24 hours" );

    liquidity_struct lp_details = get_liquidity_info( c, ds );

    if( !lp_details.is_in_range && s.last_liquidity_addition_time < now() - ( days_to_seconds(7) ) ){

    	bool can_rebalance = false;

    	if( s.wax_bucket > ZERO_WAX ){
    		s.wax_available_for_rentals += s.wax_bucket;
    		s.wax_bucket = ZERO_WAX;
    		can_rebalance = true;
    	}

    	if( s.lswax_bucket.amount > 0 && s.lswax_bucket >= ds.minimum_unliquify_amount && ds.wax_available_for_rentals.amount > 0 ){

    		int64_t max_redeemable = calculate_lswax_output( ds.wax_available_for_rentals.amount, ds );

    		if( max_redeemable > ds.minimum_unliquify_amount.amount ){
    			can_rebalance = true;

	    		int64_t amount_to_transfer = std::min( s.lswax_bucket.amount, max_redeemable );

	    		check( amount_to_transfer > 0, "can not transfer this amount" );

	    		s.lswax_bucket.amount -= amount_to_transfer;

	    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("instant redeem") );
	    	}

    	}

    	check( can_rebalance, "can not rebalance, either due to empty buckets or dapp contract not having instant redemption funds" );

		s.last_rebalance_time = now();
		state_s_3.set(s, _self);
		return;

    } else if( lp_details.is_in_range ){

    	if( s.wax_bucket > ZERO_WAX && s.lswax_bucket == ZERO_LSWAX ){

    		int64_t amount_to_transfer = calculate_asset_share( s.wax_bucket.amount, 50000000 );

    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );

    		s.wax_bucket.amount -= amount_to_transfer;
    		s.last_rebalance_time = now();
			state_s_3.set(s, _self);
			return;	

    	} else if( s.lswax_bucket > ZERO_LSWAX && s.wax_bucket == ZERO_WAX ){

    		check( s.lswax_bucket >= ds.minimum_unliquify_amount && ds.wax_available_for_rentals.amount > 0, "can not rebalance because we're unable to instant redeem lswax for wax" );

    		int64_t max_redeemable = calculate_lswax_output( ds.wax_available_for_rentals.amount, ds );

    		check( max_redeemable > ds.minimum_unliquify_amount.amount, 
    			( DAPP_CONTRACT.to_string() + " doesn't have enough wax in the instant redemption pool to rebalance " )
    			.c_str() );	 	    		

    		int64_t max_output_amount = s.lswax_bucket.amount > 0 ? calculate_swax_output( s.lswax_bucket.amount, ds ) : 0;

    		int64_t max_weight = std::min( max_output_amount, max_redeemable );

    		check( max_weight > 1, "division would result in a nonpositive quantity" );

    		int64_t weighted_amount_to_transfer = safecast::div( max_weight, int64_t(2) );

    		int64_t amount_to_transfer = calculate_lswax_output( weighted_amount_to_transfer, ds );

    		check( amount_to_transfer > 0, "can not transfer this amount" );	    		

    		s.lswax_bucket.amount -= amount_to_transfer;

    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("rebalance") );

	    	s.last_rebalance_time = now();
			state_s_3.set(s, _self);
			return;	    	    

    	} else {

    		int64_t weighted_lswax_bucket = calculate_swax_output( s.lswax_bucket.amount, ds );

    		int64_t total_weight = safecast::add( weighted_lswax_bucket, s.wax_bucket.amount );

    		int64_t half_weight = safecast::div( total_weight, int64_t(2) );

    		if( half_weight > weighted_lswax_bucket ){

    			int64_t amount_to_transfer = safecast::sub( half_weight, weighted_lswax_bucket );
    			check( amount_to_transfer >= 500000000, "amount to rebalance is too small" );

	    		s.wax_bucket.amount -= amount_to_transfer;

	    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );

    			s.last_rebalance_time = now();
				state_s_3.set(s, _self);
				return;	    							

    		} else if( half_weight > s.wax_bucket.amount ){

    			int64_t weight_difference = safecast::sub( half_weight, s.wax_bucket.amount );
    			int64_t difference_adjusted = calculate_lswax_output( weight_difference, ds );

    			int64_t max_redeemable = 
    				ds.wax_available_for_rentals.amount > 0 ?
    					calculate_lswax_output( ds.wax_available_for_rentals.amount, ds )
    				: 0;

    			int64_t amount_to_transfer = std::min( difference_adjusted, max_redeemable );
    			check( amount_to_transfer >= 500000000, "amount to rebalance is too small" );

	    		s.lswax_bucket.amount -= amount_to_transfer;

	    		transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("rebalance") );

    			s.last_rebalance_time = now();
				state_s_3.set(s, _self);
				return;	   

    		}
    	}

    } else {
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