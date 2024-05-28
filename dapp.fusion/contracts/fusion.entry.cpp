#include "fusion.hpp"
#include "functions.cpp"
#include "integer_functions.cpp"
#include "safe.cpp"
#include "on_notify.cpp"

//contractName: fusion


ACTION fusion::addadmin(const eosio::name& admin_to_add){
	require_auth(_self);
	check( is_account(admin_to_add), "admin_to_add is not a wax account" );

	config3 c = config_s_3.get();

	if( std::find( c.admin_wallets.begin(), c.admin_wallets.end(), admin_to_add ) == c.admin_wallets.end() ){
		c.admin_wallets.push_back( admin_to_add );
		config_s_3.set(c, _self);
	} else {
		check( false, ( admin_to_add.to_string() + " is already an admin" ).c_str() );
	}
}

ACTION fusion::addcpucntrct(const eosio::name& contract_to_add){
	require_auth(_self);
	check( is_account(contract_to_add), "contract_to_add is not a wax account" );

	config3 c = config_s_3.get();

	if( std::find( c.cpu_contracts.begin(), c.cpu_contracts.end(), contract_to_add ) == c.cpu_contracts.end() ){
		c.cpu_contracts.push_back( contract_to_add );
		config_s_3.set(c, _self);
	} else {
		check( false, ( contract_to_add.to_string() + " is already a cpu contract" ).c_str() );
	}
}

ACTION fusion::claimaslswax(const eosio::name& user, const eosio::asset& expected_output, const uint64_t& max_slippage_1e6){

	require_auth(user);

	//fetch the global state
	config3 c = config_s_3.get();
	state s = states.get();	
	state3 s3 = state_s_3.get();

	sync_epoch( c, s );

	//fetch the user and process any pending payouts
	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );

	//validate the action inputs and make sure they have a claimable balance
    check( expected_output.amount > 0, "Invalid output quantity." );
    check( expected_output.amount < MAX_ASSET_AMOUNT, "output quantity too large" );
    check( expected_output.symbol == LSWAX_SYMBOL, "output symbol should be LSWAX" );	 	
    check( max_slippage_1e6 >= 0 && max_slippage_1e6 < ONE_HUNDRED_PERCENT_1E6, "max slippage is out of range" );
	check( staker.claimable_wax.amount > 0, "you have no wax to claim" );

	//calculate how much lswax they will get, and make sure it's enough to meet what they expect
	int64_t claimable_wax_amount = staker.claimable_wax.amount;
	int64_t converted_lsWAX_i64 = internal_liquify( claimable_wax_amount, s );	
	uint64_t minimum_output_percentage = ONE_HUNDRED_PERCENT_1E6 - max_slippage_1e6;
	int64_t minimum_output = calculate_asset_share( expected_output.amount, minimum_output_percentage );
    check( converted_lsWAX_i64 >= (int64_t) minimum_output, "output would be " + asset(converted_lsWAX_i64, LSWAX_SYMBOL).to_string() + " but expected " + asset(minimum_output, LSWAX_SYMBOL).to_string() );		

    //update the user's row
	staker_t.modify(staker_itr, same_payer, [&](auto &_s){
		_s.claimable_wax.amount = 0;
		_s.last_update = staker.last_update;
	});

	//update the state
	s.liquified_swax.amount = safeAddInt64(s.liquified_swax.amount, converted_lsWAX_i64);
	s.swax_currently_backing_lswax.amount = safeAddInt64(s.swax_currently_backing_lswax.amount, claimable_wax_amount);
    s.wax_available_for_rentals.amount = safeAddInt64(s.wax_available_for_rentals.amount, claimable_wax_amount);
    states.set(s, _self);	

	//update the global state with the new total claimable wax amount
	debit_total_claimable_wax( s3, staker.claimable_wax );
	state_s_3.set(s3, _self);    	

    //issue the wax and lswax
    issue_swax(claimable_wax_amount);
    issue_lswax(converted_lsWAX_i64, user);

	return;
	
}

ACTION fusion::claimgbmvote(const eosio::name& cpu_contract)
{
	check( is_cpu_contract(cpu_contract), ( cpu_contract.to_string() + " is not a cpu rental contract").c_str() );
	action(permission_level{get_self(), "active"_n}, cpu_contract,"claimgbmvote"_n,std::tuple{}).send();
}

/** claimrefunds
 *  checks the system contract to see if we have any refunds to claim from unstaked CPU
 *  allowing funds to come back into the main contract so user's can redeem in a timely fashion
 */

ACTION fusion::claimrefunds()
{
	//anyone can call this

	config3 c = config_s_3.get();

	bool refund_is_available = false;

	for(eosio::name ctrct : c.cpu_contracts){
		refunds_table refunds_t = refunds_table( SYSTEM_CONTRACT, ctrct.value );

		auto refund_itr = refunds_t.find( ctrct.value );

		if( refund_itr != refunds_t.end() && refund_itr->request_time + seconds(REFUND_DELAY_SEC) <= current_time_point() ){
			action(permission_level{get_self(), "active"_n}, ctrct,"claimrefund"_n,std::tuple{}).send();
			refund_is_available = true;
		}

	}

	check( refund_is_available, "there are no refunds to claim" );
}

/** claimrewards
 *  if a user has sWAX staked, and has earned wax rewards,
 *  this will allow them to claim their wax
 */

ACTION fusion::claimrewards(const eosio::name& user){
	require_auth(user);

	//fetch the global config and states
	config3 c = config_s_3.get();
	state s = states.get();
	state3 s3 = state_s_3.get();

	sync_epoch( c, s );

	//fetch the user and process any pending payouts
	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );

	//make sure the user has something to claim
	check( staker.claimable_wax.amount > 0, "you have no wax to claim" );
	const asset claimable_wax = staker.claimable_wax;

	//modify the user's row
	staker_t.modify(staker_itr, same_payer, [&](auto &_s){
		_s.claimable_wax.amount = 0;
		_s.last_update = staker.last_update;
	});

	//update the global state with the new total claimable wax amount
	debit_total_claimable_wax( s3, claimable_wax );
	states.set(s, _self);
	state_s_3.set(s3, _self);

	//send the wax rewards to the user
	transfer_tokens( user, claimable_wax, WAX_CONTRACT, std::string("your sWAX reward claim from waxfusion.io - liquid staking protocol") );

	return;
}

/** 
* claimswax
* allows a user to compound their sWAX by claiming WAX and turning it back into more sWAX 
*/

ACTION fusion::claimswax(const eosio::name& user){
	require_auth(user);

	//fetch the global config and states
	config3 c = config_s_3.get();
	state s = states.get();
	state3 s3 = state_s_3.get();

	sync_epoch( c, s );

	//fetch the user and process any pending payouts
	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );

	//make sure the user has something to claim
	check( staker.claimable_wax.amount > 0, "you have no wax to claim" );
	int64_t swax_amount_to_claim = staker.claimable_wax.amount;

	//modify the user's row
	staker_t.modify(staker_itr, same_payer, [&](auto &_s){
		_s.claimable_wax.amount = 0;
		_s.swax_balance.amount = safeAddInt64(_s.swax_balance.amount, swax_amount_to_claim);
		_s.last_update = staker.last_update;
	});

	//update the state
    s.swax_currently_earning.amount = safeAddInt64(s.swax_currently_earning.amount, swax_amount_to_claim);
    s.wax_available_for_rentals.amount = safeAddInt64(s.wax_available_for_rentals.amount, swax_amount_to_claim);

	//update the global state with the new total claimable wax amount
	debit_total_claimable_wax( s3, staker.claimable_wax );
	states.set(s, _self);	
	state_s_3.set(s3, _self);

    //issue the swax
	issue_swax(swax_amount_to_claim);     	

	return;

}

/**
* clearexpired
* allows a user to erase their expired redemption requests from the redemptions table
*/  

ACTION fusion::clearexpired(const eosio::name& user){
	require_auth(user);

	//fetch the global config and state
	config3 c = config_s_3.get();
	state s = states.get();	

	//set the state to make sure it reflects the current epoch data
	sync_epoch( c, s );
	states.set(s, _self);

	//if there are no pending requests, throw an error
	requests_tbl requests_t = requests_tbl(get_self(), user.value);
	check( requests_t.begin() != requests_t.end(), "there are no requests to clear" );

	uint64_t upper_bound = s.last_epoch_start_time - c.seconds_between_epochs - 1;

	auto itr = requests_t.begin();
	while (itr != requests_t.end()) {
		if (itr->epoch_id >= upper_bound) break;

		itr = requests_t.erase(itr);
	}

}

/**
* createfarms
* can be called by anyone
* distributes incentives from the incentives_bucket into new alcor farms
*/

ACTION fusion::createfarms(){

	//fetch the global config and states
	config3 c = config_s_3.get();
	state s = states.get();
	state2 s2 = state_s_2.get();

	//set the state to make sure it reflects the current epoch data
	sync_epoch( c, s );
	states.set(s, _self);

	//make sure enough time has passed since last distribution, and that there are funds to use for incentives
	check( s2.last_incentive_distribution + LP_FARM_DURATION_SECONDS < now(), "hasn't been 1 week since last farms were created");
	check( s2.incentives_bucket.amount > 0, "no lswax in the incentives_bucket" );

	//we have to know what the ID of each incentive will be on alcor's contract before submitting
	//the transaction. we can do this by fetching the last row from alcor's incentives table,
	//and then incementing its ID by 1. this is what "next_key" is for
	uint64_t next_key = 0;
	auto it = incentives_t.end();

	if( incentives_t.begin() != incentives_t.end() ){
		it --;
	    next_key = it->id+ 1;
	}

	int64_t total_lswax_allocated = 0;

	//loop through the lp farms table and create the necessary farms on alcor
	for(auto lp_itr = lpfarms_t.begin(); lp_itr != lpfarms_t.end(); lp_itr++){

		//calculate how much of the incentives_bucket should go to this pair
		int64_t lswax_allocation_i64 = calculate_asset_share( s2.incentives_bucket.amount, lp_itr->percent_share_1e6 );
		
		//keep track of how much lswax we've allocated so we can verify the amounts after this loop
		total_lswax_allocated = safeAddInt64( total_lswax_allocated, lswax_allocation_i64 );

		//call the `newincentive` action on alcor's contract
		create_alcor_farm(lp_itr->poolId, lp_itr->symbol_to_incentivize, lp_itr->contract_to_incentivize);

		//construct a memo and transfer the reward tokens to alcor after the farm is created
		const std::string memo = "incentreward#" + std::to_string( next_key );
		transfer_tokens( ALCOR_CONTRACT, asset(lswax_allocation_i64, LSWAX_SYMBOL), TOKEN_CONTRACT, memo );

		next_key ++;	
	}	

	//make sure we are not using more funds for incentives than the amount in the incentives_bucket
	check(total_lswax_allocated <= s2.incentives_bucket.amount, "overallocation of incentives_bucket");

	//set the global state
	s2.incentives_bucket.amount = safeSubInt64( s2.incentives_bucket.amount, total_lswax_allocated );
	s2.last_incentive_distribution = now();
	state_s_2.set(s2, _self);
	
}

/**
* distribute action
* anyone can call this as long as 24 hours have passed since the last reward distribution
*/ 

ACTION fusion::distribute(){

	//fetch the global config and states
	config3 c = config_s_3.get();
	state s = states.get();
	state2 s2 = state_s_2.get();	

	sync_epoch( c, s );

	//make sure its been long enough since the last distribution
	if( s.next_distribution > now() ){
		check( false, ("next distribution is not until " + std::to_string(s.next_distribution) ).c_str() );
	}

	//if there is nothing to distribute, create a snapshot with 0 quantities
	if( s.revenue_awaiting_distribution.amount == 0 ){
		zero_distribution( s );
		s.next_distribution += c.seconds_between_distributions;
		states.set(s, _self);   		
		return;
	}

	//total waiting to be distributed
	int64_t amount_to_distribute = s.revenue_awaiting_distribution.amount;

	//amount to allocate to both compounding LSWAX, and rewarding SWAX stakers
	int64_t user_alloc_i64 = calculate_asset_share( amount_to_distribute, c.user_share_1e6 );

	//amount to distribute to the POL contract
	int64_t pol_alloc_i64 = calculate_asset_share( amount_to_distribute, c.pol_share_1e6 );

	//amount that goes to LP incentives for ecosystem tokens
	int64_t eco_alloc_i64 = calculate_asset_share( amount_to_distribute, c.ecosystem_share_1e6 );

	//total swax in existence, the user allocation all goes to this
	int64_t sum_of_sWAX_and_lsWAX = safeAddInt64( s.swax_currently_earning.amount, s.swax_currently_backing_lswax.amount );

	//the amount of rewards that goes to users who are staking SWAX (goes into the user funds bucket)
	int64_t swax_earning_alloc_i64 = internal_get_swax_allocations( user_alloc_i64, s.swax_currently_earning.amount, sum_of_sWAX_and_lsWAX );
	
	//the amount of wax that should buy new swax, and use it to increase the backing of LSWAX
	//this is the amount of swax that should be minted, plus the amount used for LP incentives
	//the issued swax should be added to the swax_currently_backing_lswax bucket
	int64_t swax_autocompounding_alloc_i64 = internal_get_swax_allocations( user_alloc_i64, s.swax_currently_backing_lswax.amount, sum_of_sWAX_and_lsWAX );

	//calculate swax to issue for autocompounding, and for LP incentives
	int64_t swax_amount_to_issue = safeAddInt64( swax_autocompounding_alloc_i64, eco_alloc_i64 );

	//calculate lswax to issue for the lp incentives bucket, using the ecosystem allocation amount
	int64_t lswax_amount_to_issue = internal_liquify( eco_alloc_i64, s );
	
	//make sure that the amounts we are distributing are not greater than the current reward pool
	validate_distribution_amounts(user_alloc_i64, pol_alloc_i64, eco_alloc_i64, swax_autocompounding_alloc_i64,
      swax_earning_alloc_i64, amount_to_distribute);

	//create a snapshot of this distribution
	create_snapshot(s, swax_earning_alloc_i64, swax_autocompounding_alloc_i64, pol_alloc_i64, eco_alloc_i64, amount_to_distribute);	

	//modify the state with updated info
	s.total_revenue_distributed.amount = safeAddInt64(s.total_revenue_distributed.amount, amount_to_distribute);
	s.next_distribution += c.seconds_between_distributions;
    s.wax_available_for_rentals.amount = safeAddInt64(s.wax_available_for_rentals.amount, swax_amount_to_issue);
	s.revenue_awaiting_distribution.amount = 0;
	s.user_funds_bucket.amount = safeAddInt64( s.user_funds_bucket.amount, swax_earning_alloc_i64);
	s2.incentives_bucket.amount = safeAddInt64( s2.incentives_bucket.amount, lswax_amount_to_issue );	
	s.swax_currently_backing_lswax.amount = safeAddInt64( s.swax_currently_backing_lswax.amount, eco_alloc_i64 );
	s.liquified_swax.amount = safeAddInt64( s.liquified_swax.amount, lswax_amount_to_issue );  
	s.swax_currently_backing_lswax.amount = safeAddInt64( s.swax_currently_backing_lswax.amount, swax_autocompounding_alloc_i64 );  

	//set the state
    states.set(s, _self);
    state_s_2.set(s2, _self);

    //issue the swax and lswax
	issue_lswax( lswax_amount_to_issue, _self );
	issue_swax( swax_amount_to_issue );    

	//pol share goes to POL_CONTRACT
	transfer_tokens( POL_CONTRACT, asset(pol_alloc_i64, WAX_SYMBOL), WAX_CONTRACT, std::string("pol allocation from waxfusion distribution") );    

	return;	

}


ACTION fusion::initconfig(){
	require_auth(get_self());
	config3 c = config_s_3.get();

	eosio::check(!states.exists(), "State already exists");

	state s{};
	s.swax_currently_earning = ZERO_SWAX;
	s.swax_currently_backing_lswax = ZERO_SWAX;
	s.liquified_swax = ZERO_LSWAX;
	s.revenue_awaiting_distribution = ZERO_WAX;
	s.user_funds_bucket = ZERO_WAX;
	s.total_revenue_distributed = ZERO_WAX;
	s.next_distribution = INITIAL_EPOCH_START_TIMESTAMP + 60 * 60 * 24; /* 1 day from launch */
	s.wax_for_redemption = ZERO_WAX;
	s.redemption_period_start = 0;
	s.redemption_period_end = 0;
	s.last_epoch_start_time = INITIAL_EPOCH_START_TIMESTAMP;
	s.wax_available_for_rentals = ZERO_WAX;
	s.cost_to_rent_1_wax = asset(120000, WAX_SYMBOL); /* 0.01 WAX per day */
	s.current_cpu_contract = "cpu1.fusion"_n;
	s.next_stakeall_time = INITIAL_EPOCH_START_TIMESTAMP + 60 * 60 * 24; /* 1 day */
	states.set(s, _self);

	//create the first epoch
	create_epoch( c, INITIAL_EPOCH_START_TIMESTAMP, "cpu1.fusion"_n, ZERO_WAX );

	return;
}


ACTION fusion::initconfig3(){
	require_auth( _self );

	eosio::check(!config_s_3.exists(), "Config3 already exists");

	config3 c{};
	c.minimum_stake_amount = eosio::asset(100000000, WAX_SYMBOL);
	c.minimum_unliquify_amount = eosio::asset(100000000, LSWAX_SYMBOL);
	c.seconds_between_distributions = days_to_seconds(1);
	c.max_snapshots_to_process = 180;
	c.initial_epoch_start_time = INITIAL_EPOCH_START_TIMESTAMP;
	c.cpu_rental_epoch_length_seconds = days_to_seconds(14); /* 14 days */
	c.seconds_between_epochs = days_to_seconds(7); /* 7 days */
	c.user_share_1e6 = 85 * SCALE_FACTOR_1E6;
	c.pol_share_1e6 = 7 * SCALE_FACTOR_1E6;
	c.ecosystem_share_1e6 = 8 * SCALE_FACTOR_1E6;
	c.admin_wallets = {
		"guild.waxdao"_n,
		"oig"_n,
		_self,
		"admin.wax"_n
	};
	c.cpu_contracts = {
		"cpu1.fusion"_n,
		"cpu2.fusion"_n,
		"cpu3.fusion"_n
	};
	c.redemption_period_length_seconds = days_to_seconds(2); /* 2 days */
	c.seconds_between_stakeall = days_to_seconds(1); /* once per day */
	c.fallback_cpu_receiver = "updatethings"_n;
	config_s_3.set(c, _self);
	
}

ACTION fusion::initstate2(){
	require_auth(get_self());

	eosio::check(!state_s_2.exists(), "State2 already exists");

	state2 s2{};
	s2.last_incentive_distribution = 0;
	s2.incentives_bucket = ZERO_LSWAX;
	s2.total_value_locked = ZERO_WAX;
	s2.total_shares_allocated = 0;
	state_s_2.set(s2, _self);
}

ACTION fusion::initstate3(){
	require_auth(get_self());

	eosio::check(!state_s_3.exists(), "State3 already exists");

	eosio::asset total_claimable_wax = ZERO_WAX;

	/** total wax owed:
	 *  available_for_rentals
	 *  revenue_awaiting_distribution
	 *  claimable_wax
	 *  wax_for_redemption
	 *  user_funds_bucket
	 */


	if(staker_t.begin() != staker_t.end()){
		for(auto itr = staker_t.begin(); itr != staker_t.end(); itr++){
			total_claimable_wax.amount = safeAddInt64( total_claimable_wax.amount, itr->claimable_wax.amount );
		}
	}

	state3 s3{};
	s3.total_claimable_wax = total_claimable_wax;
	s3.total_wax_owed = ZERO_WAX;
	s3.contract_wax_balance = ZERO_WAX;
	s3.last_update = 0;

	state_s_3.set(s3, _self);
}

ACTION fusion::inittop21(){
	require_auth(get_self());

	eosio::check(!top21_s.exists(), "top21 already exists");

	auto idx = _producers.get_index<"prototalvote"_n>();	


	using value_type = std::pair<eosio::producer_authority, uint16_t>;
	std::vector< value_type > top_producers;
	top_producers.reserve(21);	


	/** my mock action to emplace producers (initproducer) on the mock
	 *  system contract is causing some 
	 *  weird behavior with indexing by prototalvote, so am just 
	 *  looping through the whole table here and checking if each
	 *  producer meets the criteria
	 *  
	 *  the `updatetop21` action in this file uses the correct logic for
	 *  using prototalvote secondary index instead of this loop below.
	 *  when deployed on testnet/mainnet, this action below will be updated
	 * 
	 * 	^ TODO
	 *  
	 *  the below loop is good enough for unit testing environment
	 *  as its not important that the mock system contract be identical
	 *  to the core system contract
	 */

	for( auto it = _producers.begin(); it != _producers.end() 
			&& 0 < it->total_votes 
			; ++it ) {

		if(top_producers.size() == 21) break;
		if(it->is_active){

			top_producers.emplace_back(
				eosio::producer_authority{
				   .producer_name = it->owner,
				   .authority     = it->get_producer_authority()
				},
				it->location
			);	

		}

	}

	if( top_producers.size() < MINIMUM_PRODUCERS_TO_VOTE_FOR ) {
		check( false, ("attempting to vote for " + std::to_string( top_producers.size() ) + " producers but need to vote for " + std::to_string( MINIMUM_PRODUCERS_TO_VOTE_FOR ) ).c_str() );
	}	

	std::sort(top_producers.begin(), top_producers.end(),
	    [](const value_type& a, const value_type& b) -> bool {
	        return a.first.producer_name.to_string() < b.first.producer_name.to_string();
	    }
	);	

	std::vector<eosio::name> producers_to_vote_for {};

	for(auto t : top_producers){
		producers_to_vote_for.push_back(t.first.producer_name);
	}

	top21 t{};
	t.block_producers = producers_to_vote_for;
	t.last_update = now();
	top21_s.set(t, _self);

}

/** 
 *  instaredeem
 *  by default, when users request redemptions, they get added to the queue
 *  depending on which epochs have the wax available for redemption
 *  there is 0 fee
 *  the instaredeem action allows users to redeem instantly using funds that 
 *  are inside the dapp contract (avaiable_for_rentals pool), assuming there are 
 *  enough funds to cover their redemption
 *  there is a 0.05% fee when using instaredeem. this allows the protocol to utilize
 *  funds that would normally be used for CPU rentals and staking, in a more efficient manner
 *  while also helping to maintain the LSWAX peg on the open market
 */
ACTION fusion::instaredeem(const eosio::name& user, const eosio::asset& swax_to_redeem){
	require_auth(user);

	//fetch the global config and state
	config3 c = config_s_3.get();
	state s = states.get();
	
	sync_epoch( c, s );

	//make sure the asset symbol is valid
	check( swax_to_redeem.symbol == SWAX_SYMBOL, "symbol mismatch on swax_to_redeem" );

	//fetch the user data and process any pending payouts
	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );

	check( staker.swax_balance >= swax_to_redeem, "you are trying to redeem more than you have" );

    check( swax_to_redeem.amount > 0, "Must redeem a positive quantity" );
    check( swax_to_redeem.amount < MAX_ASSET_AMOUNT, "quantity too large" );

    staker.swax_balance.amount = safeSubInt64( staker.swax_balance.amount, swax_to_redeem.amount );

	//debit requested amount from their staked balance
	staker_t.modify(staker_itr, same_payer, [&](auto &_s){
		_s.swax_balance = staker.swax_balance;
		_s.last_update = staker.last_update;
	});

	//make sure there is enough wax in the rental pool to cover this request
	check( s.wax_available_for_rentals.amount >= swax_to_redeem.amount, "not enough instaredeem funds available" );

	//calculate the 0.05% share
	//calculate the remainder for the user
	uint64_t user_percentage_1e6 = ONE_HUNDRED_PERCENT_1E6 - PROTOCOL_FEE_1E6;
	int64_t protocol_share = calculate_asset_share( swax_to_redeem.amount, PROTOCOL_FEE_1E6 );
	int64_t user_share = calculate_asset_share( swax_to_redeem.amount, user_percentage_1e6 );

	//make sure the calculated amounts aren't > the requested redemption amount
	check( safeAddInt64( protocol_share, user_share ) <= swax_to_redeem.amount, "error calculating protocol fee" );

	//add the 0.05% to the revenue_awaiting_distribution
	//debit the amount from s.wax_available_for_rentals
	//debit the amount from swax_currently_earning
	s.wax_available_for_rentals.amount = safeSubInt64(s.wax_available_for_rentals.amount, swax_to_redeem.amount);	
	s.revenue_awaiting_distribution.amount = safeAddInt64( s.revenue_awaiting_distribution.amount, protocol_share );
	s.swax_currently_earning.amount = safeSubInt64( s.swax_currently_earning.amount, swax_to_redeem.amount );

	//set the state
	states.set(s, _self);

	//if they have any pending requests, make sure they dont add up to > their swax balance
    debit_user_redemptions_if_necessary(user, staker.swax_balance);

    //transfer the funds to the user and retire the swax
    retire_swax(swax_to_redeem.amount);
	transfer_tokens( user, asset( user_share, WAX_SYMBOL ), WAX_CONTRACT, std::string("your sWAX redemption from waxfusion.io - liquid staking protocol") );    

	return;
}


ACTION fusion::liquify(const eosio::name& user, const eosio::asset& quantity){
	require_auth(user);
    check(quantity.amount > 0, "Invalid quantity.");
    check(quantity.amount < MAX_ASSET_AMOUNT, "quantity too large");
    check(quantity.symbol == SWAX_SYMBOL, "only SWAX can be liquified");

    state s = states.get();	

    //fetch the staker info and process any pending payouts
	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );

	if(staker.swax_balance < quantity){
		check(false, "you are trying to liquify more than you have");
	}

	staker.swax_balance.amount = safeSubInt64( staker.swax_balance.amount, quantity.amount );

	//debit requested amount from their staked balance
	staker_t.modify(staker_itr, same_payer, [&](auto &_s){
		_s.swax_balance = staker.swax_balance;
		_s.last_update = staker.last_update;
	});

	//calculate equivalent amount of lsWAX (BEFORE adjusting sWAX amounts)
	int64_t converted_lsWAX_i64 = internal_liquify(quantity.amount, s);

	//subtract swax amount from swax_currently_earning
	s.swax_currently_earning.amount = safeSubInt64(s.swax_currently_earning.amount, quantity.amount);

	//add swax amount to swax_currently_backing_swax
	s.swax_currently_backing_lswax.amount = safeAddInt64(s.swax_currently_backing_lswax.amount, quantity.amount);

	//issue the converted lsWAX amount to the user
	issue_lswax(converted_lsWAX_i64, user);

	//add the issued amount to liquified_swax
	s.liquified_swax.amount = safeAddInt64(s.liquified_swax.amount, converted_lsWAX_i64);

	//apply the changes to state table
	states.set(s, _self);

	debit_user_redemptions_if_necessary(user, staker.swax_balance);

	return;
}

ACTION fusion::liquifyexact(const eosio::name& user, const eosio::asset& quantity, 
	const eosio::asset& expected_output, const uint64_t& max_slippage_1e6)
{

	require_auth(user);

	//validate the action inputs
    check(quantity.amount > 0, "Invalid quantity.");
    check(quantity.amount < MAX_ASSET_AMOUNT, "quantity too large");
    check(quantity.symbol == SWAX_SYMBOL, "only SWAX can be liquified");	
    check(expected_output.amount > 0, "Invalid output quantity.");
    check(expected_output.amount < MAX_ASSET_AMOUNT, "output quantity too large");
    check(expected_output.symbol == LSWAX_SYMBOL, "output symbol should be LSWAX");	   
    check( max_slippage_1e6 >= 0 && max_slippage_1e6 < ONE_HUNDRED_PERCENT_1E6, "max slippage is out of range" ); 

    //fetch the global state
    state s = states.get();

    //fetch the user info and process any pending payouts
	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );

	//make sure they aren't requesting more than their balance
	if(staker.swax_balance < quantity){
		check(false, "you are trying to liquify more than you have");
	}

	//debit requested amount from their staked balance
	staker.swax_balance.amount = safeSubInt64( staker.swax_balance.amount, quantity.amount );

	//update the user details in the stakers table
	staker_t.modify(staker_itr, same_payer, [&](auto &_s){
		_s.swax_balance = staker.swax_balance;
		_s.last_update = staker.last_update;
	});

	//calculate how much lsWAX the outcome will be
	uint64_t minimum_output_percentage = ONE_HUNDRED_PERCENT_1E6 - max_slippage_1e6;
	int64_t converted_lsWAX_i64 = internal_liquify(quantity.amount, s);
	int64_t minimum_output = calculate_asset_share( expected_output.amount, minimum_output_percentage );

	//make sure the outcome is acceptable for the user
	check( converted_lsWAX_i64 >= minimum_output, "output would be " + asset(converted_lsWAX_i64, LSWAX_SYMBOL).to_string() + " but expected " + asset(minimum_output, LSWAX_SYMBOL).to_string() );

	//subtract swax amount from swax_currently_earning
	s.swax_currently_earning.amount = safeSubInt64(s.swax_currently_earning.amount, quantity.amount);

	//add swax amount to swax_currently_backing_swax
	s.swax_currently_backing_lswax.amount = safeAddInt64(s.swax_currently_backing_lswax.amount, quantity.amount);

	//issue the converted lsWAX amount to the user
	issue_lswax(converted_lsWAX_i64, user);

	//add the issued amount to liquified_swax
	s.liquified_swax.amount = safeAddInt64(s.liquified_swax.amount, converted_lsWAX_i64);

	//apply the changes to state table
	states.set(s, _self);

	debit_user_redemptions_if_necessary(user, staker.swax_balance);

	return;
}

/**
* reallocate
* used for taking any funds that were requested to be redeemed, but werent redeemed in time.
* moves the funds back into the rental pool where they can be used for CPU rentals again
*/ 

ACTION fusion::reallocate(){

	//fetch the global state and config
	state s = states.get();
	config3 c = config_s_3.get();	

	sync_epoch( c, s );

	//if now > epoch start time + 48h, it means redemption is over
	check( now() > s.last_epoch_start_time + c.redemption_period_length_seconds, "redemption period has not ended yet" );

	//move funds from redemption pool to rental pool
	check( s.wax_for_redemption.amount > 0, "there is no wax to reallocate" );

	//empty the redemption pool and move it into the rental pool so the funds an be utilized again
	s.wax_available_for_rentals.amount = safeAddInt64(s.wax_available_for_rentals.amount, s.wax_for_redemption.amount);
	s.wax_for_redemption = ZERO_WAX;

	states.set(s, _self);
}

ACTION fusion::redeem(const eosio::name& user){
	require_auth(user);

	//fetch the global state and config
	state s = states.get();
	config3 c = config_s_3.get();	

	sync_epoch( c, s );

	//locate the user and process any pending payouts
	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );	

	//calculate the start/end time of the current redemption period, and the epoch related to it
	uint64_t redemption_start_time = s.last_epoch_start_time;
	uint64_t redemption_end_time = s.last_epoch_start_time + c.redemption_period_length_seconds;
	uint64_t epoch_to_claim_from = s.last_epoch_start_time - c.cpu_rental_epoch_length_seconds;
 
 	//make sure the redemption window is in progress
	check( now() < redemption_end_time, 
		( "next redemption does not start until " + std::to_string(s.last_epoch_start_time + c.seconds_between_epochs) ).c_str() 
	);

	//make sure the user has a redemption request for this perios
	requests_tbl requests_t = requests_tbl(get_self(), user.value);
	auto req_itr = requests_t.require_find(epoch_to_claim_from, "you don't have a redemption request for the current redemption period");

	//this should never happen because the amounts were validated when the redemption requests were created
	//this is just a safety check
	check( req_itr->wax_amount_requested.amount <= staker.swax_balance.amount, "you are trying to redeem more than you have" );

	//make sure s.wax_for_redemption has enough for them (it always should!)
	check( s.wax_for_redemption >= req_itr->wax_amount_requested, "not enough wax in the redemption pool" );

	//subtract the amount from s.wax_for_redemption
	s.wax_for_redemption.amount = safeSubInt64(s.wax_for_redemption.amount, req_itr->wax_amount_requested.amount);

	//subtract the requested amount from their swax balance
	staker.swax_balance.amount = safeSubInt64(staker.swax_balance.amount, req_itr->wax_amount_requested.amount);

	staker_t.modify(staker_itr, same_payer, [&](auto &_s){
		_s.swax_balance = staker.swax_balance;
		_s.last_update = staker.last_update;
	});

	//update the swax_currently_earning amount
	s.swax_currently_earning.amount = safeSubInt64(s.swax_currently_earning.amount, req_itr->wax_amount_requested.amount);
	states.set(s, _self);

	//retire the sWAX
	retire_swax(req_itr->wax_amount_requested.amount);

	//transfer wax to the user
	transfer_tokens( user, req_itr->wax_amount_requested, WAX_CONTRACT, std::string("your sWAX redemption from waxfusion.io - liquid staking protocol") );

	//erase the request
	req_itr = requests_t.erase(req_itr);
}


ACTION fusion::removeadmin(const eosio::name& admin_to_remove){
	require_auth(_self);

	config3 c = config_s_3.get();

    auto itr = std::remove(c.admin_wallets.begin(), c.admin_wallets.end(), admin_to_remove);

    if (itr != c.admin_wallets.end()) {
        c.admin_wallets.erase(itr, c.admin_wallets.end());
        config_s_3.set(c, _self);
    } else {
        check(false, (admin_to_remove.to_string() + " is not an admin").c_str());
    }
}

/**
* reqredeem (request redeem)
* initiates a redemption request
* the contract will automatically figure out which epoch(s) have enough wax available
* the user must claim (redeem) their wax within 2 days of it becoming available, or it will be restaked
* 
*/ 

ACTION fusion::reqredeem(const eosio::name& user, const eosio::asset& swax_to_redeem, const bool& accept_replacing_prev_requests){
	require_auth(user);

	config3 c = config_s_3.get();
	state s = states.get();
	sync_epoch( c, s );

	auto staker_itr = staker_t.require_find(user.value, "you don't have anything staked here");
	staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
	sync_user( s, staker );

    check( swax_to_redeem.amount > 0, "Must redeem a positive quantity" );
    check( swax_to_redeem.amount < MAX_ASSET_AMOUNT, "quantity too large" );

	/** 
	* figure out which epoch(s) to take this from, update the epoch(s) to reflect the request
	* first need to find out the epoch linked to the unstake action that is closest to taking place
	* does that have any wax available?
	* if so, put as much of this request into that epoch as possible
	* if there is anything left, then check the next epoch too
	* if that has anything available, repeat
	* can do this one more time if there is a 3rd epoch available
	*/

	requests_tbl requests_t = requests_tbl(get_self(), user.value);

	/** if there is currently a redemption window open, we need to check if 
	 *  this user has a request from this window. if they do, it should be
	 *  automatically claimed and transferred to them, to avoid tying up 
	 *  2x the redemption amount and causing issues for other users who 
	 *  need to redeem
	 */

	bool request_can_be_filled = false;
	eosio::asset remaining_amount_to_fill = swax_to_redeem;

	uint64_t redemption_start_time = s.last_epoch_start_time;
	uint64_t redemption_end_time = s.last_epoch_start_time + c.redemption_period_length_seconds;
	uint64_t epoch_to_claim_from = s.last_epoch_start_time - c.seconds_between_epochs;
 
	if( now() < redemption_end_time ){
		//there is currently a redemption window open
		//if this user has a request in that window, handle it before proceeding

		auto epoch_itr = epochs_t.find( epoch_to_claim_from );

		if( epoch_itr != epochs_t.end() ){

			auto req_itr = requests_t.find( epoch_to_claim_from );		

			if( req_itr != requests_t.end() ){
				//they have a request from this redemption window

				//make sure the request amount <= their swax balance
				//this should never actually happen
				check( req_itr->wax_amount_requested.amount <= staker.swax_balance.amount, "you have a pending request > your swax balance" );

				//make sure the redemption pool >= the request amount
				//the only time this should ever happen is if the CPU contract has not returned the funds yet, which should never really
				//be more than a 5-10 minute window on a given week
				check( s.wax_for_redemption.amount >= req_itr->wax_amount_requested.amount, "redemption pool is < your pending request" );

				if( req_itr->wax_amount_requested.amount >= remaining_amount_to_fill.amount ){
					request_can_be_filled = true;
				} else {
					remaining_amount_to_fill.amount = safeSubInt64( remaining_amount_to_fill.amount, req_itr->wax_amount_requested.amount );
				}

				s.wax_for_redemption.amount = safeSubInt64( s.wax_for_redemption.amount, req_itr->wax_amount_requested.amount );

				epochs_t.modify( epoch_itr, same_payer, [&](auto &_e){
					_e.wax_to_refund.amount = safeSubInt64( _e.wax_to_refund.amount, req_itr->wax_amount_requested.amount );
				});

				transfer_tokens( user, req_itr->wax_amount_requested, WAX_CONTRACT, std::string("your redemption from waxfusion.io - liquid staking protocol") );

				staker.swax_balance.amount = safeSubInt64( staker.swax_balance.amount, req_itr->wax_amount_requested.amount );

				req_itr = requests_t.erase( req_itr );

			}

		}	
	}

	if( request_can_be_filled ){
		staker_t.modify(staker_itr, same_payer, [&](auto &_s){
			_s.swax_balance = staker.swax_balance;
			_s.last_update = staker.last_update;
		});		

		states.set(s, _self);
		return;
	}

	check(staker.swax_balance >= remaining_amount_to_fill, "you are trying to redeem more than you have");	

	uint64_t epoch_to_request_from = s.last_epoch_start_time - c.seconds_between_epochs;

	std::vector<uint64_t> epochs_to_check = {
		epoch_to_request_from,
		epoch_to_request_from + c.seconds_between_epochs,
		epoch_to_request_from + ( c.seconds_between_epochs * 2 )
	};

	/** 
	* loop through the 3 redemption periods and if the user has any reqs,
	* delete them and sub the amounts from epoch_itr->wax_to_refund
	*/

	for(uint64_t ep : epochs_to_check){
		auto epoch_itr = epochs_t.find(ep);

		if(epoch_itr != epochs_t.end()){

			auto req_itr = requests_t.find(ep);	

			if(req_itr != requests_t.end()){
				//there is a pending request

				if(!accept_replacing_prev_requests){
					check(false, "you have previous requests but passed 'false' to the accept_replacing_prev_requests param");
				}

				//subtract the pending amount from epoch_itr->wax_to_refund
				int64_t updated_refunding_amount = safeSubInt64(epoch_itr->wax_to_refund.amount, req_itr->wax_amount_requested.amount);

				epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
					_e.wax_to_refund = asset(updated_refunding_amount, WAX_SYMBOL);
				});

				//erase the request
				req_itr = requests_t.erase(req_itr);
			}
		}	
	}

	/**
	* now loop through them again and process them
	* if request becomes filled, break out of the loop
	*/

	for(uint64_t ep : epochs_to_check){
		auto epoch_itr = epochs_t.find(ep);

		if(epoch_itr != epochs_t.end()){

			//see if the deadline for redeeming has passed yet
			if(epoch_itr->redemption_period_start_time > now() &&  epoch_itr->wax_to_refund < epoch_itr->wax_bucket ){

				//there are still funds available for redemption

				int64_t amount_available = safeSubInt64(epoch_itr->wax_bucket.amount, epoch_itr->wax_to_refund.amount);

				if(amount_available >= remaining_amount_to_fill.amount){
					//this epoch has enough to cover the whole request
					request_can_be_filled = true;

					int64_t updated_refunding_amount = safeAddInt64(epoch_itr->wax_to_refund.amount, remaining_amount_to_fill.amount);

					//add the amount to the epoch's wax_to_refund
					epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
						_e.wax_to_refund = asset(updated_refunding_amount, WAX_SYMBOL);
					});

					/** 
					* INSERT this request into the request_tbl
					* (any previous records should have been deleted first)
					*/

					auto req_itr = requests_t.find(ep);

					check( req_itr == requests_t.end(), "user has an existing redemption request in this epoch" );

					requests_t.emplace(user, [&](auto &_r){
						_r.epoch_id = ep;
						_r.wax_amount_requested = asset(remaining_amount_to_fill.amount, WAX_SYMBOL);
					});


				} else {
					//this epoch has some funds, but not enough for the whole request
					int64_t updated_refunding_amount = safeAddInt64(epoch_itr->wax_to_refund.amount, amount_available);

					//debit the amount remaining so we are checking an updated number on the next loop
					remaining_amount_to_fill.amount = safeSubInt64(remaining_amount_to_fill.amount, amount_available);

					epochs_t.modify(epoch_itr, get_self(), [&](auto &_e){
						_e.wax_to_refund = asset(updated_refunding_amount, WAX_SYMBOL);
					});

					auto req_itr = requests_t.find(ep);

					check( req_itr == requests_t.end(), "user has an existing redemption request in this epoch" );
					
					requests_t.emplace(user, [&](auto &_r){
						_r.epoch_id = ep;
						_r.wax_amount_requested = asset(amount_available, WAX_SYMBOL);
					});
				}
			}

		}	

		if( request_can_be_filled ) break;
	}	 

	if( !request_can_be_filled ){
		/** make sure there is enough wax in available_for_rentals pool to cover the remainder
		 *  there should be 0 cases where this this check fails
		 *  if epochs cant cover it, there should always be enough in available for rentals to 
		 *  cover the remainder. because if the funds aren't staked in epochs, the only other 
		 *  place they can be is in the rental pool.
		 */ 

		check( s.wax_available_for_rentals.amount >= remaining_amount_to_fill.amount, "Request amount is greater than amount in epochs and rental pool" );

		s.wax_available_for_rentals.amount = safeSubInt64( s.wax_available_for_rentals.amount, remaining_amount_to_fill.amount );

		//debit the swax amount from the user's balance
		staker_t.modify(staker_itr, same_payer, [&](auto &_s){
			_s.swax_balance.amount = safeSubInt64( staker.swax_balance.amount, remaining_amount_to_fill.amount );
			_s.last_update = staker.last_update;
		});

		transfer_tokens( user, asset( remaining_amount_to_fill.amount, WAX_SYMBOL ), WAX_CONTRACT, std::string("your redemption from waxfusion.io - liquid staking protocol") );
	}

	states.set(s, _self);
	return;

}

ACTION fusion::rmvcpucntrct(const eosio::name& contract_to_remove){
	require_auth(_self);

	config3 c = config_s_3.get();

    auto itr = std::remove(c.cpu_contracts.begin(), c.cpu_contracts.end(), contract_to_remove);

    if (itr != c.cpu_contracts.end()) {
        c.cpu_contracts.erase(itr, c.cpu_contracts.end());
        config_s_3.set(c, _self);
    } else {
        check(false, (contract_to_remove.to_string() + " is not a cpu contract").c_str());
    }
}

ACTION fusion::rmvincentive(const uint64_t& poolId){
	require_auth( _self );

	//fetch the global state
	state2 s2 = state_s_2.get(); 	

	auto lp_itr = lpfarms_t.require_find( poolId, "this poolId doesn't exist in the lpfarms table" );

	s2.total_shares_allocated = safeSubUInt64( s2.total_shares_allocated, lp_itr->percent_share_1e6 );

	lp_itr = lpfarms_t.erase( lp_itr );

	//apply the global state change
	state_s_2.set(s2, _self);
}

/** setfallback
 *  every 24 hours, unrented CPU funds are automatically staked to an epoch, in order
 *  to generate rewards and maximize APR. since they are not rented, the CPU needs to 
 *  be staked to an account. the fallback_cpu_receiver is the account that will
 *  get the CPU
 */

ACTION fusion::setfallback(const eosio::name& caller, const eosio::name& receiver){
	require_auth(caller);
	check( is_an_admin(caller), "this action requires auth from one of the admin_wallets in the config table" );
	check( is_account(receiver), "cpu receiver is not a wax account" );

	config3 c = config_s_3.get();
	c.fallback_cpu_receiver = receiver;
	config_s_3.set(c, _self);
}

/** setincentive
 *  adds/modifies an LP pair to receive rewards on alcor, from the lp_incentives bucket
 */

ACTION fusion::setincentive(const uint64_t& poolId, const eosio::symbol& symbol_to_incentivize, const eosio::name& contract_to_incentivize, const uint64_t& percent_share_1e6){
	require_auth( _self );
	check(percent_share_1e6 > 0, "percent_share_1e6 must be positive");

	//fetch the global state
	state2 s2 = state_s_2.get(); 

	auto itr = pools_t.require_find(poolId, "this poolId does not exist");

	if( (itr->tokenA.quantity.symbol != symbol_to_incentivize && itr->tokenA.contract != contract_to_incentivize)
		&&
		(itr->tokenB.quantity.symbol != symbol_to_incentivize && itr->tokenB.contract != contract_to_incentivize)

	){
		check(false, "this poolId does not contain the symbol/contract combo you entered");
	}

	if(symbol_to_incentivize == LSWAX_SYMBOL && contract_to_incentivize == TOKEN_CONTRACT){
		check(false, "LSWAX cannot be paired against itself");
	}

	auto lp_itr = lpfarms_t.find( poolId );

	if(lp_itr == lpfarms_t.end()){

		s2.total_shares_allocated = safeAddUInt64( s2.total_shares_allocated, percent_share_1e6 );

		lpfarms_t.emplace(_self, [&](auto &_lp){
			_lp.poolId = poolId;
  			_lp.symbol_to_incentivize = symbol_to_incentivize;
  			_lp.contract_to_incentivize = contract_to_incentivize;
  			_lp.percent_share_1e6 = percent_share_1e6;
		});

	} else {

		//make sure a change is being made
		check( lp_itr->percent_share_1e6 != percent_share_1e6, "the share you entered is the same as the existing share" );

		//calculate the difference in global shares
		if( lp_itr->percent_share_1e6 > percent_share_1e6 ){
			//subtract from global shares
			uint64_t difference = safeSubUInt64( lp_itr->percent_share_1e6, percent_share_1e6 );
			s2.total_shares_allocated = safeSubUInt64( s2.total_shares_allocated, difference );
		} else {
			//add to global shares
			uint64_t difference = safeSubUInt64( percent_share_1e6, lp_itr->percent_share_1e6 );
			s2.total_shares_allocated = safeAddUInt64( s2.total_shares_allocated, difference );
		}

		lpfarms_t.modify(lp_itr, _self, [&](auto &_lp){
			_lp.poolId = poolId;
  			_lp.symbol_to_incentivize = symbol_to_incentivize;
  			_lp.contract_to_incentivize = contract_to_incentivize;
  			_lp.percent_share_1e6 = percent_share_1e6;
		});

	}

	//validate that the global incentive shares are not > 100%, and apply the state change
	check( s2.total_shares_allocated <= ONE_HUNDRED_PERCENT_1E6, "total shares can not be > 100%" );
	state_s_2.set( s2, _self );

}


/**
* setpolshare
* Adjusts the percentage of revenue that goes to POL, within a limited range of 5-10%
*/

ACTION fusion::setpolshare(const uint64_t& pol_share_1e6){
	require_auth( _self );
	check( pol_share_1e6 >= 5 * SCALE_FACTOR_1E6 && pol_share_1e6 <= 10 * SCALE_FACTOR_1E6, "acceptable range is 5-10%" );

	config3 c = config_s_3.get();
	c.pol_share_1e6 = pol_share_1e6;
	config_s_3.set(c, _self);	
}

/** setrentprice
 *  updates the cost of renting CPU from the protocol
 *  also inline updates the CPU cost on the POL contract
 */

ACTION fusion::setrentprice(const eosio::name& caller, const eosio::asset& cost_to_rent_1_wax){
	require_auth(caller);
	check( is_an_admin(caller), "this action requires auth from one of the admin_wallets in the config table" );
	check( cost_to_rent_1_wax.amount > 0, "cost must be positive" );
	check( cost_to_rent_1_wax.symbol == WAX_SYMBOL, "symbol and precision must match WAX" );

	state s = states.get();
	s.cost_to_rent_1_wax = cost_to_rent_1_wax;
	states.set(s, _self);

	action(permission_level{get_self(), "active"_n}, POL_CONTRACT,"setrentprice"_n,std::tuple{ cost_to_rent_1_wax }).send();
}

/**
* stake
* this just opens a row if necessary so we can react to transfers etc
* also syncs the user if they exist
*/
  
ACTION fusion::stake(const eosio::name& user){
	require_auth(user);
	state s = states.get();

	auto staker_itr = staker_t.find(user.value);

	if(staker_itr != staker_t.end()){
		staker_struct staker = { user, staker_itr->swax_balance, staker_itr->claimable_wax, staker_itr->last_update };
		sync_user( s, staker );

		staker_t.modify(staker_itr, same_payer, [&](auto &_s){
			_s.claimable_wax = staker.claimable_wax;
			_s.last_update = staker.last_update;
		});

		states.set(s, _self);
		return;
	}

	staker_t.emplace(user, [&](auto &_s){
		_s.wallet = user;
		_s.swax_balance = ZERO_SWAX;
		_s.claimable_wax = ZERO_WAX;
		_s.last_update = now();
	});
}

/**
* stakeallcpu
* once every 24h, this can be called to take any un-rented wax and just stake it so it earns the normal amount
*/ 

ACTION fusion::stakeallcpu(){
	state s = states.get();
	config3 c = config_s_3.get();

	sync_epoch( c, s );

	//if now > epoch start time + 48h, it means redemption is over
	check( now() >= s.next_stakeall_time, ( "next stakeall time is not until " + std::to_string(s.next_stakeall_time) ).c_str() );

	if(s.wax_available_for_rentals.amount > 0){

		//then we can just get the next contract in line and next epoch in line
		eosio::name next_cpu_contract = get_next_cpu_contract( c, s );

		uint64_t next_epoch_start_time = s.last_epoch_start_time + c.seconds_between_epochs;

		transfer_tokens( next_cpu_contract, s.wax_available_for_rentals, WAX_CONTRACT, cpu_stake_memo(c.fallback_cpu_receiver, next_epoch_start_time) );

		//upsert the epoch that it was staked to, so it reflects the added wax
		auto next_epoch_itr = epochs_t.find(next_epoch_start_time);

		if(next_epoch_itr == epochs_t.end()){
			//create new epoch, set the wax bucket to the amount we are about to stake
			create_epoch( c, next_epoch_start_time, next_cpu_contract, s.wax_available_for_rentals );
		} else {
			//update epoch
			asset current_wax_bucket = next_epoch_itr->wax_bucket;
			current_wax_bucket.amount = safeAddInt64(current_wax_bucket.amount, s.wax_available_for_rentals.amount);
			epochs_t.modify(next_epoch_itr, get_self(), [&](auto &_e){
				_e.wax_bucket = current_wax_bucket;
			});
		}

		//reset rental pool to 0
		s.wax_available_for_rentals = ZERO_WAX;
	}

	//update the next_stakeall_time
	s.next_stakeall_time += c.seconds_between_stakeall;
	states.set(s, _self);
}

/**
* sync
* this only exists to keep data refreshed and make it easier for front ends to display fresh data
* it's not necessary for the dapp to function properly
* therefore it requires admin auth to avoid random people spamming the network and running this constantly
*/ 

ACTION fusion::sync(const eosio::name& caller){
	require_auth( caller );
	check( is_an_admin( caller ), ( caller.to_string() + " is not an admin" ).c_str() );
	
	state s = states.get();
	config3 c = config_s_3.get();

	sync_epoch( c, s );
	states.set(s, _self);
}

/**
* synctvl
* aggregates data related to amount of WAX locked across all protocol contracts
*/ 

ACTION fusion::synctvl(const eosio::name& caller){
	require_auth( caller );
	check( is_an_admin( caller ), ( caller.to_string() + " is not an admin" ).c_str() );
	sync_tvl();
}


ACTION fusion::unstakecpu(const uint64_t& epoch_id, const int& limit){
	//anyone can call this
	state s = states.get();
	config3 c = config_s_3.get();

	sync_epoch( c, s );

	states.set(s, _self);

	//the only epoch that should ever need unstaking is the one that started prior to current epoch
	//calculate the epoch prior to the most recently started one
	//this can be overridden by specifying an epoch_id in the action instead of passing 0
	uint64_t epoch_to_check = epoch_id == 0 ? s.last_epoch_start_time - c.seconds_between_epochs : epoch_id;

	//if the unstake time is <= now, look up its cpu contract is delband table
	auto epoch_itr = epochs_t.require_find( epoch_to_check, ("could not find epoch " + std::to_string( epoch_to_check ) ).c_str() );

	check( epoch_itr->time_to_unstake <= now(), ("can not unstake until another " + std::to_string( epoch_itr-> time_to_unstake - now() ) + " seconds has passed").c_str() );

	del_bandwidth_table del_tbl( SYSTEM_CONTRACT, epoch_itr->cpu_wallet.value );

	if( del_tbl.begin() == del_tbl.end() ){
		check( false, ( epoch_itr->cpu_wallet.to_string() + " has nothing to unstake" ).c_str() );
	}	

	int rows_limit = limit == 0 ? 500 : limit;

	action(permission_level{get_self(), "active"_n}, epoch_itr->cpu_wallet,"unstakebatch"_n,std::tuple{ rows_limit }).send();

	renters_table renters_t = renters_table( _self, epoch_to_check );

	if( renters_t.begin() == renters_t.end() ){
		return;
	}

	int count = 0;
	auto rental_itr = renters_t.begin();
	while (rental_itr != renters_t.end()) {
		if (count == rows_limit) return;
		rental_itr = renters_t.erase( rental_itr );
		count ++;
	}

}

ACTION fusion::updatetop21(){
	top21 t = top21_s.get();

	check( t.last_update + (60 * 60 * 24) <= now(), "hasn't been 24h since last top21 update" );

	auto idx = _producers.get_index<"prototalvote"_n>();	

	using value_type = std::pair<eosio::producer_authority, uint16_t>;
	std::vector< value_type > top_producers;
	top_producers.reserve(21);	

	for( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
		top_producers.emplace_back(
			eosio::producer_authority{
			   .producer_name = it->owner,
			   .authority     = it->get_producer_authority()
			},
			it->location
		);
	}

	if( top_producers.size() < MINIMUM_PRODUCERS_TO_VOTE_FOR ) {
		check( false, ("attempting to vote for " + std::to_string( top_producers.size() ) + " producers but need to vote for " + std::to_string( MINIMUM_PRODUCERS_TO_VOTE_FOR ) ).c_str() );
	}	

	std::sort(top_producers.begin(), top_producers.end(),
	    [](const value_type& a, const value_type& b) -> bool {
	        return a.first.producer_name.to_string() < b.first.producer_name.to_string();
	    }
	);	

	std::vector<eosio::name> producers_to_vote_for {};

	for(auto t : top_producers){
		producers_to_vote_for.push_back(t.first.producer_name);
	}

	t.block_producers = producers_to_vote_for;
	t.last_update = now();
	top21_s.set(t, _self);
}