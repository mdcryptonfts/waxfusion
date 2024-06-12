#include "fusion.hpp"
#include "functions.cpp"
#include "integer_functions.cpp"
#include "safe.cpp"
#include "on_notify.cpp"
#include "staking.cpp"
#include "redemptions.cpp"

//contractName: fusion

ACTION fusion::addadmin(const eosio::name& admin_to_add) {
	require_auth(_self);
	check( is_account(admin_to_add), "admin_to_add is not a wax account" );

	global g = global_s.get();

	check( std::find( g.admin_wallets.begin(), g.admin_wallets.end(), admin_to_add ) == g.admin_wallets.end(), ( admin_to_add.to_string() + " is already an admin" ).c_str() );
	
	g.admin_wallets.push_back( admin_to_add );
	global_s.set(g, _self);
}

ACTION fusion::addcpucntrct(const eosio::name& contract_to_add) {
	require_auth(_self);
	check( is_account(contract_to_add), "contract_to_add is not a wax account" );

	global g = global_s.get();

	check( std::find( g.cpu_contracts.begin(), g.cpu_contracts.end(), contract_to_add ) == g.cpu_contracts.end(), ( contract_to_add.to_string() + " is already a cpu contract" ).c_str() );
	g.cpu_contracts.push_back( contract_to_add );
	global_s.set(g, _self);
}

/** claimaslswax
 * 	allows the user to claim their claimable wax,
 * 	convert it to swax and then liquify the swax into lswax
 *  in a single transaction
 *  lswax received will be based on the current ratio of wax/lswax,
 *  not the full claimable wax balance
 *  swax will be issued 1:1 with the claimable balance
 *  lswax will be issued at the ratio of wax/lswax
 */

ACTION fusion::claimaslswax(const eosio::name& user, const eosio::asset& minimum_output) {

	require_auth(user);

	global g = global_s.get();
	rewards r = rewards_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);	

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);

	check( minimum_output > ZERO_LSWAX, "Invalid output quantity." );
	check( minimum_output.amount < MAX_ASSET_AMOUNT, "output quantity too large" );
	check( staker.claimable_wax > ZERO_WAX, "you have no wax to claim" );

	//calculate how much lswax they will get, and make sure it's enough to meet what they expect
	int64_t claimable_wax_amount = staker.claimable_wax.amount;
	int64_t converted_lsWAX_i64 = calculate_lswax_output( claimable_wax_amount, g );
	check( converted_lsWAX_i64 >= minimum_output.amount, "output would be " + asset(converted_lsWAX_i64, LSWAX_SYMBOL).to_string() + " but expected " + minimum_output.to_string() );

	staker.claimable_wax = ZERO_WAX;
	self_staker.swax_balance += asset(claimable_wax_amount, SWAX_SYMBOL);

	modify_staker(staker);
	modify_staker(self_staker);

	r.totalSupply += uint128_t(claimable_wax_amount);

	g.liquified_swax += asset(converted_lsWAX_i64, LSWAX_SYMBOL);
	g.swax_currently_backing_lswax += asset(claimable_wax_amount, SWAX_SYMBOL);
	g.wax_available_for_rentals += asset(claimable_wax_amount, WAX_SYMBOL);
	g.total_rewards_claimed += asset(claimable_wax_amount, WAX_SYMBOL);

	rewards_s.set(r, _self);	
	global_s.set(g, _self);

	issue_swax(claimable_wax_amount);
	issue_lswax(converted_lsWAX_i64, user);

}

ACTION fusion::claimgbmvote(const eosio::name& cpu_contract)
{
	global g = global_s.get();
	check( is_cpu_contract(g, cpu_contract), ( cpu_contract.to_string() + " is not a cpu rental contract").c_str() );
	action(permission_level{get_self(), "active"_n}, cpu_contract, "claimgbmvote"_n, std::tuple{}).send();
}

/** claimrefunds
 *  checks the system contract to see if we have any refunds to claim from unstaked CPU
 *  allowing funds to come back into the main contract so user's can redeem in a timely fashion
 */

ACTION fusion::claimrefunds()
{
	//anyone can call this

	global g = global_s.get();

	bool refund_is_available = false;

	for (name ctrct : g.cpu_contracts) {
		refunds_table refunds_t = refunds_table( SYSTEM_CONTRACT, ctrct.value );

		auto refund_itr = refunds_t.find( ctrct.value );

		if ( refund_itr != refunds_t.end() && refund_itr->request_time + seconds(REFUND_DELAY_SEC) <= current_time_point() ) {
			action(permission_level{get_self(), "active"_n}, ctrct, "claimrefund"_n, std::tuple{}).send();
			refund_is_available = true;
		}

	}

	check( refund_is_available, "there are no refunds to claim" );
}

/** claimrewards
 *  if a user has sWAX staked, and has earned wax rewards,
 *  this will allow them to claim their wax
 */

ACTION fusion::claimrewards(const eosio::name& user) {

	require_auth(user);

	global g = global_s.get();
	rewards r = rewards_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);

	check( staker.claimable_wax > ZERO_WAX, "you have no wax to claim" );
	asset claimable_wax = staker.claimable_wax;
	staker.claimable_wax = ZERO_WAX;

	modify_staker(staker);
	modify_staker(self_staker);	

	g.total_rewards_claimed += claimable_wax;	

	global_s.set(g, _self);
	rewards_s.set(r, _self);

	transfer_tokens( user, claimable_wax, WAX_CONTRACT, std::string("your sWAX reward claim from waxfusion.io - liquid staking protocol") );
}

/**
* claimswax
* allows a user to compound their sWAX by claiming WAX and turning it back into more sWAX
* will result in new sWAX being minted at a 1:1 ratio with the user's claimable balance
*/

ACTION fusion::claimswax(const eosio::name& user) {

	require_auth(user);

	global g = global_s.get();
	rewards r = rewards_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);

	check( staker.claimable_wax > ZERO_WAX, "you have no wax to claim" );
	int64_t swax_amount_to_claim = staker.claimable_wax.amount;

	r.totalSupply += uint128_t(swax_amount_to_claim);

	staker.claimable_wax = ZERO_WAX;
	staker.swax_balance += asset(swax_amount_to_claim, SWAX_SYMBOL);

	modify_staker(staker);
	modify_staker(self_staker);

	g.swax_currently_earning.amount += swax_amount_to_claim;
	g.wax_available_for_rentals.amount += swax_amount_to_claim;
	g.total_rewards_claimed.amount += swax_amount_to_claim;

	rewards_s.set(r, _self);
	global_s.set(g, _self);

	issue_swax(swax_amount_to_claim);
}

/**
* clearexpired
* allows a user to erase their expired redemption requests from the redemptions table
*/

ACTION fusion::clearexpired(const eosio::name& user) {
	require_auth(user);

	global g = global_s.get();

	sync_epoch( g );

	requests_tbl requests_t = requests_tbl(get_self(), user.value);
	check( requests_t.begin() != requests_t.end(), "there are no requests to clear" );

	uint64_t upper_bound = g.last_epoch_start_time - g.seconds_between_epochs - 1;

	auto itr = requests_t.begin();
	while (itr != requests_t.end()) {
		if (itr->epoch_id >= upper_bound) break;

		itr = requests_t.erase(itr);
	}

	global_s.set(g, _self);
}

/** compound action
 *  can be called by anyone, once every 5 mins
 *  claims any rewards accrued by self_staker
 *  uses them to buy more sWAX and increase the self_stake
 *  also increases swax_currently_backing_lswax,
 *  which is essentially autocompounding of lsWAX value
 */

ACTION fusion::compound(){

	global g = global_s.get();
	rewards r = rewards_s.get();

	check( g.last_compound_time + (5*60) <= now(), "must wait 5 minutes between compounds" );

	sync_epoch( g );	

	auto self_staker_itr = staker_t.require_find( _self.value, ERR_STAKER_NOT_FOUND );
	staker_struct self_staker = staker_struct(*self_staker_itr);	

	extend_reward(g, r, self_staker);
	update_reward(self_staker, r);	

	int64_t amount_to_compound = self_staker.claimable_wax.amount;
	check( amount_to_compound > 0, "nothing to compound" );	
	
	self_staker.swax_balance.amount += amount_to_compound;
	self_staker.claimable_wax = ZERO_WAX;
	modify_staker(self_staker);		

	r.totalSupply += uint128_t(amount_to_compound);

	g.swax_currently_backing_lswax.amount += amount_to_compound;
	g.total_rewards_claimed.amount += amount_to_compound;
	g.last_compound_time = now();

	issue_swax( amount_to_compound );

	rewards_s.set(r, _self);
	global_s.set(g, _self);	

}

/**
* createfarms
* can be called by anyone
* distributes incentives from the incentives_bucket into new alcor farms
*/

ACTION fusion::createfarms() {

	global g = global_s.get();

	sync_epoch( g );

	check( g.last_incentive_distribution + LP_FARM_DURATION_SECONDS <= now(), "hasn't been 1 week since last farms were created");
	check( g.incentives_bucket > ZERO_LSWAX, "no lswax in the incentives_bucket" );

	//we have to know what the ID of each incentive will be on alcor's contract before submitting
	//the transaction. we can do this by fetching the last row from alcor's incentives table,
	//and then incementing its ID by 1. this is what "next_key" is for
	uint64_t next_key = 0;
	auto it = incentives_t.end();

	if ( incentives_t.begin() != incentives_t.end() ) {
		it --;
		next_key = it->id + 1;
	}

	int64_t total_lswax_allocated = 0;

	for (auto lp_itr = lpfarms_t.begin(); lp_itr != lpfarms_t.end(); lp_itr++) {

		int64_t lswax_allocation_i64 = calculate_asset_share( g.incentives_bucket.amount, lp_itr->percent_share_1e6 );

		total_lswax_allocated = safecast::add( total_lswax_allocated, lswax_allocation_i64 );

		create_alcor_farm(lp_itr->poolId, lp_itr->symbol_to_incentivize, lp_itr->contract_to_incentivize);

		const std::string memo = "incentreward#" + std::to_string( next_key );
		transfer_tokens( ALCOR_CONTRACT, asset(lswax_allocation_i64, LSWAX_SYMBOL), TOKEN_CONTRACT, memo );

		next_key ++;
	}

	check(total_lswax_allocated <= g.incentives_bucket.amount, "overallocation of incentives_bucket");

	g.incentives_bucket.amount = safecast::sub( g.incentives_bucket.amount, total_lswax_allocated );
	g.last_incentive_distribution = now();
	global_s.set(g, _self);

}

/** init
 *  initializes the global and rewards singletons
 *  also emplaces the self_staker which holds all swax_backing_lswax
 */

ACTION fusion::init(const asset& initial_reward_pool){
	require_auth( _self );

	accounts wax_table = accounts( WAX_CONTRACT, _self.value );
	auto balance_itr = wax_table.require_find( WAX_SYMBOL.code().raw(), "no WAX balance found" );
	check( balance_itr->balance >= initial_reward_pool, "we don't have enough WAX to cover the initial_reward_pool" );

	check( !global_s.exists(), "global singleton already exists" );
	check( !rewards_s.exists(), "rewards singleton already exists" );

	global g{};
	g.swax_currently_earning = ZERO_SWAX;
	g.swax_currently_backing_lswax = ZERO_SWAX;
	g.liquified_swax = ZERO_LSWAX;
	g.revenue_awaiting_distribution = ZERO_WAX;
	g.total_revenue_distributed = initial_reward_pool;
	g.wax_for_redemption = ZERO_WAX;
	g.last_epoch_start_time = now();
	g.wax_available_for_rentals = ZERO_WAX;
	g.cost_to_rent_1_wax = asset(120000, WAX_SYMBOL); /* 0.01 WAX per day */
	g.current_cpu_contract = "cpu1.fusion"_n;
	g.next_stakeall_time = now() + 60 * 60 * 24;	
	g.last_incentive_distribution = 0;
	g.incentives_bucket = ZERO_LSWAX;
	g.total_value_locked = initial_reward_pool;
	g.total_shares_allocated = 0;
	g.minimum_stake_amount = eosio::asset(100000000, WAX_SYMBOL);
	g.last_compound_time = now();
	g.minimum_unliquify_amount = eosio::asset(100000000, LSWAX_SYMBOL);
	g.initial_epoch_start_time = now();
	g.cpu_rental_epoch_length_seconds = days_to_seconds(14); /* 14 days */
	g.seconds_between_epochs = days_to_seconds(7); /* 7 days */
	g.user_share_1e6 = 85 * SCALE_FACTOR_1E6;
	g.pol_share_1e6 = 7 * SCALE_FACTOR_1E6;
	g.ecosystem_share_1e6 = 8 * SCALE_FACTOR_1E6;
	g.admin_wallets = {
		"guild.waxdao"_n,
		"oig"_n,
		_self,
		"admin.wax"_n
	};
	g.cpu_contracts = {
		"cpu1.fusion"_n,
		"cpu2.fusion"_n,
		"cpu3.fusion"_n
	};
	g.redemption_period_length_seconds = days_to_seconds(2); /* 2 days */
	g.seconds_between_stakeall = days_to_seconds(1); /* once per day */
	g.fallback_cpu_receiver = "updatethings"_n;
	g.protocol_fee_1e6 = 50000; /* 0.05% */
	g.total_rewards_claimed = ZERO_WAX;
	global_s.set(g, _self);

	create_epoch( g, now(), "cpu1.fusion"_n, ZERO_WAX );

	staker_t.emplace(_self, [&](auto &row){
		row.wallet = _self;
		row.swax_balance = ZERO_SWAX;
		row.last_update = now();
		row.claimable_wax = ZERO_WAX;
		row.userRewardPerTokenPaid = 0;		
	});

	rewards r{};
	r.periodStart = now() + (60*60*6); /* 6 hours from now */
	r.periodFinish = now() + (60*60*6) + STAKING_FARM_DURATION;
	r.rewardRate = mulDiv128( uint128_t(initial_reward_pool.amount), SCALE_FACTOR_1E8, uint128_t(STAKING_FARM_DURATION) );
	r.rewardsDuration = STAKING_FARM_DURATION;
	r.lastUpdateTime = now();
	r.rewardPerTokenStored = 0;
	r.rewardPool = initial_reward_pool;
	r.totalSupply = 0;	
	r.totalRewardsPaidOut = ZERO_WAX;
	rewards_s.set(r, _self);		
}


ACTION fusion::inittop21() {
	require_auth(get_self());

	check(!top21_s.exists(), "top21 already exists");

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
	 *
	 *  see here to verify that the implentation on testnet is confirmed working:
	 *  https://testnet.waxblock.io/account/dapp.fusion?code=dapp.fusion&scope=dapp.fusion&table=top21&lower_bound=&upper_bound=&limit=10&reverse=false#contract-tables
	 */

	for ( auto it = _producers.begin(); it != _producers.end()
	        && 0 < it->total_votes
	        ; ++it ) {

		if (top_producers.size() == 21) break;
		if (it->is_active) {

			top_producers.emplace_back(
			eosio::producer_authority{
				.producer_name = it->owner,
				.authority     = it->get_producer_authority()
			},
			it->location
			);

		}

	}

	if ( top_producers.size() < MINIMUM_PRODUCERS_TO_VOTE_FOR ) {
		check( false, ("attempting to vote for " + std::to_string( top_producers.size() ) + " producers but need to vote for " + std::to_string( MINIMUM_PRODUCERS_TO_VOTE_FOR ) ).c_str() );
	}

	std::sort(top_producers.begin(), top_producers.end(),
	[](const value_type & a, const value_type & b) -> bool {
		return a.first.producer_name.to_string() < b.first.producer_name.to_string();
	}
	         );

	std::vector<eosio::name> producers_to_vote_for {};

	for (auto t : top_producers) {
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
 *  the user's sWAX balance will be retired during this transaction
 */
ACTION fusion::instaredeem(const eosio::name& user, const eosio::asset& swax_to_redeem) {

	require_auth(user);

	rewards r = rewards_s.get();	
	global g = global_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);		

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);

	r.totalSupply -= uint128_t(swax_to_redeem.amount);

	check( staker.swax_balance >= swax_to_redeem, "you are trying to redeem more than you have" );
	check( swax_to_redeem > ZERO_SWAX, "Must redeem a positive quantity" );
	check( swax_to_redeem.amount < MAX_ASSET_AMOUNT, "quantity too large" );
	check( g.wax_available_for_rentals.amount >= swax_to_redeem.amount, "not enough instaredeem funds available" );

	staker.swax_balance -= swax_to_redeem;

	modify_staker(staker);
	modify_staker(self_staker);	

	int64_t protocol_share = calculate_asset_share( swax_to_redeem.amount, g.protocol_fee_1e6 );
	int64_t user_share = safecast::sub(swax_to_redeem.amount, protocol_share);

	check( safecast::add( protocol_share, user_share ) <= swax_to_redeem.amount, "error calculating protocol fee" );

	g.wax_available_for_rentals.amount = safecast::sub(g.wax_available_for_rentals.amount, swax_to_redeem.amount);
	g.revenue_awaiting_distribution.amount = safecast::add( g.revenue_awaiting_distribution.amount, protocol_share );
	g.swax_currently_earning.amount = safecast::sub( g.swax_currently_earning.amount, swax_to_redeem.amount );

	rewards_s.set(r, _self);
	global_s.set(g, _self);

	debit_user_redemptions_if_necessary(user, staker.swax_balance);
	retire_swax(swax_to_redeem.amount);
	transfer_tokens( user, asset( user_share, WAX_SYMBOL ), WAX_CONTRACT, std::string("your sWAX redemption from waxfusion.io - liquid staking protocol") );
}

/** liquify
 *  used when a user wants to convert their sWAX into lsWAX
 *  new lsWAX will be minted based on the current ratio of wax/lsWAX
 *	the user's sWAX balance will be moved to swax_currently_backing_lswax
 */

ACTION fusion::liquify(const eosio::name& user, const eosio::asset& quantity) {

	require_auth(user);
	check(quantity > ZERO_SWAX, "Invalid quantity.");
	check(quantity.amount < MAX_ASSET_AMOUNT, "quantity too large");

	rewards r = rewards_s.get();
	global g = global_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);	

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);

	check( staker.swax_balance >= quantity, "you are trying to liquify more than you have" );

	staker.swax_balance -= quantity;
	self_staker.swax_balance += quantity;

	modify_staker(staker);
	modify_staker(self_staker);

	int64_t converted_lsWAX_i64 = calculate_lswax_output(quantity.amount, g);

	g.swax_currently_earning -= quantity;
	g.swax_currently_backing_lswax += quantity;
	g.liquified_swax.amount += converted_lsWAX_i64;

	issue_lswax(converted_lsWAX_i64, user);
	debit_user_redemptions_if_necessary(user, staker.swax_balance);

	rewards_s.set(r, _self);
	global_s.set(g, _self);
}

/** liquifyexact
 *  same as liquify, but allows the user to specify the minimum amount received
 */

ACTION fusion::liquifyexact(const eosio::name& user, const eosio::asset& quantity,
                            const eosio::asset& minimum_output)
{

	require_auth(user);

	check(quantity > ZERO_SWAX, "Invalid quantity.");
	check(minimum_output > ZERO_LSWAX, "Invalid output quantity.");
	check(quantity.amount < MAX_ASSET_AMOUNT, "quantity too large");
	check(minimum_output.amount < MAX_ASSET_AMOUNT, "output quantity too large");

	rewards r = rewards_s.get();
	global g = global_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);	

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);

	check( staker.swax_balance >= quantity, "you are trying to liquify more than you have" );

	staker.swax_balance -= quantity;
	self_staker.swax_balance += quantity;

	modify_staker(staker);
	modify_staker(self_staker);

	int64_t converted_lsWAX_i64 = calculate_lswax_output(quantity.amount, g);
	check( converted_lsWAX_i64 >= minimum_output.amount, "output would be " + asset(converted_lsWAX_i64, LSWAX_SYMBOL).to_string() + " but expected " + minimum_output.to_string() );

	g.swax_currently_earning -= quantity;
	g.swax_currently_backing_lswax += quantity;
	g.liquified_swax.amount += converted_lsWAX_i64;

	rewards_s.set(r, _self);
	global_s.set(g, _self);

	issue_lswax(converted_lsWAX_i64, user);
	debit_user_redemptions_if_necessary(user, staker.swax_balance);
}

/**
* reallocate
* used for taking any funds that were requested to be redeemed, but werent redeemed in time.
* moves the funds back into the rental pool where they can be used for CPU rentals again
*/

ACTION fusion::reallocate() {

	global g = global_s.get();

	sync_epoch( g );

	check( now() > g.last_epoch_start_time + g.redemption_period_length_seconds, "redemption period has not ended yet" );
	check( g.wax_for_redemption > ZERO_WAX, "there is no wax to reallocate" );

	g.wax_available_for_rentals += g.wax_for_redemption;
	g.wax_for_redemption = ZERO_WAX;

	global_s.set(g, _self);
}

/** redeem
 *  a user has requested a redemption, and is now claiming it
 *  they will get their wax back, and the sWAX they were holding will be retired
 */

ACTION fusion::redeem(const eosio::name& user) {

	require_auth(user);

	rewards r = rewards_s.get();
	global g = global_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);		

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);
	modify_staker(self_staker);

	uint64_t redemption_start_time = g.last_epoch_start_time;
	uint64_t redemption_end_time = g.last_epoch_start_time + g.redemption_period_length_seconds;
	uint64_t epoch_to_claim_from = g.last_epoch_start_time - g.cpu_rental_epoch_length_seconds;

	check( now() < redemption_end_time,
	       ( "next redemption does not start until " + std::to_string(g.last_epoch_start_time + g.seconds_between_epochs) ).c_str()
	     );

	requests_tbl requests_t = requests_tbl(get_self(), user.value);
	auto req_itr = requests_t.require_find(epoch_to_claim_from, "you don't have a redemption request for the current redemption period");

	//this should never happen because the amounts were validated when the redemption requests were created
	//this is just a safety check
	check( req_itr->wax_amount_requested.amount <= staker.swax_balance.amount, "you are trying to redeem more than you have" );
	check( g.wax_for_redemption >= req_itr->wax_amount_requested, "not enough wax in the redemption pool" );

	r.totalSupply -= uint128_t(req_itr->wax_amount_requested.amount);
	staker.swax_balance -= asset(req_itr->wax_amount_requested.amount, SWAX_SYMBOL);
	modify_staker(staker);

	g.wax_for_redemption -= req_itr->wax_amount_requested;
	g.swax_currently_earning.amount -= req_itr->wax_amount_requested.amount;
	
	rewards_s.set(r, _self);
	global_s.set(g, _self);

	retire_swax(req_itr->wax_amount_requested.amount);
	transfer_tokens( user, req_itr->wax_amount_requested, WAX_CONTRACT, std::string("your sWAX redemption from waxfusion.io - liquid staking protocol") );

	requests_t.erase(req_itr);
}


ACTION fusion::removeadmin(const eosio::name& admin_to_remove) {
	require_auth(_self);

	global g = global_s.get();

	auto itr = std::remove(g.admin_wallets.begin(), g.admin_wallets.end(), admin_to_remove);

	check( itr != g.admin_wallets.end(), (admin_to_remove.to_string() + " is not an admin").c_str() );
	
	g.admin_wallets.erase(itr, g.admin_wallets.end());
	global_s.set(g, _self);

}

/**
* reqredeem (request redeem)
* initiates a redemption request
* the contract will automatically figure out which epoch(s) have enough wax available
* the user must claim (redeem) their wax within 2 days of it becoming available, or it will be restaked
* 
* helper functions can be found in redemptions.cpp
*
*/

ACTION fusion::reqredeem(const eosio::name& user, const eosio::asset& swax_to_redeem, const bool& accept_replacing_prev_requests) {

	require_auth(user);

	rewards r = rewards_s.get();
	global g = global_s.get();

	sync_epoch( g );

	auto [staker, self_staker] = get_stakers(user, _self);	

	extend_reward(g, r, self_staker);
	update_reward(staker, r);
	update_reward(self_staker, r);
	modify_staker(self_staker);	

	check( swax_to_redeem > ZERO_SWAX, "Must redeem a positive quantity" );
	check( swax_to_redeem.amount < MAX_ASSET_AMOUNT, "quantity too large" );

	bool request_can_be_filled = false;
	asset remaining_amount_to_fill = swax_to_redeem;

	//in redemptions.cpp
	handle_available_request( g, request_can_be_filled, staker, remaining_amount_to_fill );

	if ( request_can_be_filled ) {
		modify_staker(staker);
		rewards_s.set(r, _self);
		global_s.set(g, _self);
		return;
	}

	check( staker.swax_balance >= remaining_amount_to_fill, "you are trying to redeem more than you have" );

	std::vector<uint64_t> epochs_to_check = {
		g.last_epoch_start_time - g.seconds_between_epochs,
		g.last_epoch_start_time,
		g.last_epoch_start_time + g.seconds_between_epochs
	};

	//in redemptions.cpp
	remove_existing_requests( epochs_to_check, staker, accept_replacing_prev_requests );
	handle_new_request( epochs_to_check, request_can_be_filled, staker, remaining_amount_to_fill );

	if ( !request_can_be_filled ) {
		/** make sure there is enough wax in available_for_rentals pool to cover the remainder
		 *  there should be 0 cases where this this check fails
		 *  if epochs cant cover it, there should always be enough in available for rentals to
		 *  cover the remainder. because if the funds aren't staked in epochs, the only other
		 *  place they can be is in the rental pool.
		 */

		check( g.wax_available_for_rentals.amount >= remaining_amount_to_fill.amount, "Request amount is greater than amount in epochs and rental pool" );

		g.wax_available_for_rentals.amount -= remaining_amount_to_fill.amount;
		staker.swax_balance -= remaining_amount_to_fill;
		transfer_tokens( user, asset( remaining_amount_to_fill.amount, WAX_SYMBOL ), WAX_CONTRACT, std::string("your redemption from waxfusion.io - liquid staking protocol") );
	}

	modify_staker(staker);
	rewards_s.set(r, _self);
	global_s.set(g, _self);
}

ACTION fusion::rmvcpucntrct(const eosio::name& contract_to_remove) {
	require_auth(_self);

	global g = global_s.get();

	auto itr = std::remove(g.cpu_contracts.begin(), g.cpu_contracts.end(), contract_to_remove);

	check( itr != g.cpu_contracts.end(), (contract_to_remove.to_string() + " is not a cpu contract").c_str() );

	g.cpu_contracts.erase(itr, g.cpu_contracts.end());
	global_s.set(g, _self);
}

ACTION fusion::rmvincentive(const uint64_t& poolId) {
	require_auth( _self );

	global g = global_s.get();

	auto lp_itr = lpfarms_t.require_find( poolId, "this poolId doesn't exist in the lpfarms table" );

	g.total_shares_allocated = safecast::sub( g.total_shares_allocated, lp_itr->percent_share_1e6 );

	lpfarms_t.erase( lp_itr );

	global_s.set(g, _self);
}

/** setfallback
 *  every 24 hours, unrented CPU funds are automatically staked to an epoch, in order
 *  to generate rewards and maximize APR. since they are not rented, the CPU needs to
 *  be staked to an account. the fallback_cpu_receiver is the account that will
 *  get the CPU
 */

ACTION fusion::setfallback(const eosio::name& caller, const eosio::name& receiver) {
	require_auth(caller);

	global g = global_s.get();

	check( is_an_admin(g, caller), "this action requires auth from one of the admin_wallets in the global table" );
	check( is_account(receiver), "cpu receiver is not a wax account" );

	g.fallback_cpu_receiver = receiver;
	global_s.set(g, _self);
}

/** setincentive
 *  adds/modifies an LP pair to receive rewards on alcor, from the lp_incentives bucket
 */

ACTION fusion::setincentive(const uint64_t& poolId, const eosio::symbol& symbol_to_incentivize, const eosio::name& contract_to_incentivize, const uint64_t& percent_share_1e6) {
	require_auth( _self );
	check(percent_share_1e6 > 0, "percent_share_1e6 must be positive");

	global g = global_s.get();

	auto itr = pools_t.require_find(poolId, "this poolId does not exist");

	if ( (itr->tokenA.quantity.symbol != symbol_to_incentivize && itr->tokenA.contract != contract_to_incentivize)
	        &&
	        (itr->tokenB.quantity.symbol != symbol_to_incentivize && itr->tokenB.contract != contract_to_incentivize)

	   ) {
		check(false, "this poolId does not contain the symbol/contract combo you entered");
	}

	if (symbol_to_incentivize == LSWAX_SYMBOL && contract_to_incentivize == TOKEN_CONTRACT) {
		check(false, "LSWAX cannot be paired against itself");
	}

	auto lp_itr = lpfarms_t.find( poolId );

	if (lp_itr == lpfarms_t.end()) {

		g.total_shares_allocated = safecast::add( g.total_shares_allocated, percent_share_1e6 );

		lpfarms_t.emplace(_self, [&](auto & _lp) {
			_lp.poolId = poolId;
			_lp.symbol_to_incentivize = symbol_to_incentivize;
			_lp.contract_to_incentivize = contract_to_incentivize;
			_lp.percent_share_1e6 = percent_share_1e6;
		});

	} else {

		check( lp_itr->percent_share_1e6 != percent_share_1e6, "the share you entered is the same as the existing share" );

		//calculate the difference in global shares
		if ( lp_itr->percent_share_1e6 > percent_share_1e6 ) {

			uint64_t difference = safecast::sub( lp_itr->percent_share_1e6, percent_share_1e6 );
			g.total_shares_allocated = safecast::sub( g.total_shares_allocated, difference );

		} else {

			uint64_t difference = safecast::sub( percent_share_1e6, lp_itr->percent_share_1e6 );
			g.total_shares_allocated = safecast::add( g.total_shares_allocated, difference );

		}

		lpfarms_t.modify(lp_itr, _self, [&](auto & _lp) {
			_lp.poolId = poolId;
			_lp.symbol_to_incentivize = symbol_to_incentivize;
			_lp.contract_to_incentivize = contract_to_incentivize;
			_lp.percent_share_1e6 = percent_share_1e6;
		});

	}

	check( g.total_shares_allocated <= ONE_HUNDRED_PERCENT_1E6, "total shares can not be > 100%" );
	global_s.set( g, _self );

}


/**
* setpolshare
* Adjusts the percentage of revenue that goes to POL, within a limited range of 5-10%
*/

ACTION fusion::setpolshare(const uint64_t& pol_share_1e6) {
	require_auth( _self );
	check( pol_share_1e6 >= uint64_t(5 * SCALE_FACTOR_1E6) && pol_share_1e6 <= uint64_t(10 * SCALE_FACTOR_1E6), "acceptable range is 5-10%" );

	global g = global_s.get();
	g.pol_share_1e6 = pol_share_1e6;
	global_s.set(g, _self);
}

/**
* setredeemfee
* Adjusts the percentage fee for instant redemptions
* allows values between 0 and 1%
*/

ACTION fusion::setredeemfee(const uint64_t& protocol_fee_1e6) {
	require_auth( _self );
	check( protocol_fee_1e6 >= 0 && protocol_fee_1e6 <= uint64_t(SCALE_FACTOR_1E6), "acceptable range is 0-1%" );

	global g = global_s.get();
	g.protocol_fee_1e6 = protocol_fee_1e6;
	global_s.set(g, _self);
}

/** setrentprice
 *  updates the cost of renting CPU from the protocol
 *  also inline updates the CPU cost on the POL contract
 */

ACTION fusion::setrentprice(const eosio::name& caller, const eosio::asset& cost_to_rent_1_wax) {
	require_auth(caller);

	global g = global_s.get();

	check( is_an_admin(g, caller), "this action requires auth from one of the admin_wallets in the config table" );
	check( cost_to_rent_1_wax > ZERO_WAX, "cost must be positive" );

	g.cost_to_rent_1_wax = cost_to_rent_1_wax;
	global_s.set(g, _self);

	action(permission_level{get_self(), "active"_n}, POL_CONTRACT, "setrentprice"_n, std::tuple{ cost_to_rent_1_wax }).send();
}

/**
* stake
* this just opens a row if necessary so we can react to transfers etc
* also syncs the user if they exist
*/

ACTION fusion::stake(const eosio::name& user) {

	require_auth(user);

	rewards r = rewards_s.get();
	global g = global_s.get();

	sync_epoch( g );

	staker_struct staker;
	auto staker_itr = staker_t.find(user.value);

	if (staker_itr != staker_t.end()) {

		staker = staker_struct(*staker_itr);	

		auto self_staker_itr = staker_t.require_find( _self.value, ERR_STAKER_NOT_FOUND );
		staker_struct self_staker = staker_struct(*self_staker_itr);	

		extend_reward(g, r, self_staker);
		update_reward(staker, r);
		update_reward(self_staker, r);
		modify_staker(staker);
		modify_staker(self_staker);

		rewards_s.set(r, _self);
		global_s.set(g, _self);

	} else {
		staker_t.emplace(user, [&](auto & _s) {
			_s.wallet = user;
			_s.swax_balance = ZERO_SWAX;
			_s.claimable_wax = ZERO_WAX;
			_s.last_update = now();
			_s.userRewardPerTokenPaid = 0;
		});
	}
}

/**
* stakeallcpu
* once every 24h, this can be called to take any un-rented wax and just stake it so it earns the normal amount
*/

ACTION fusion::stakeallcpu() {
	
	global g = global_s.get();

	sync_epoch( g );

	check( now() >= g.next_stakeall_time, ( "next stakeall time is not until " + std::to_string(g.next_stakeall_time) ).c_str() );

	if (g.wax_available_for_rentals.amount > 0) {

		eosio::name next_cpu_contract = get_next_cpu_contract( g );

		uint64_t next_epoch_start_time = g.last_epoch_start_time + g.seconds_between_epochs;

		transfer_tokens( next_cpu_contract, g.wax_available_for_rentals, WAX_CONTRACT, cpu_stake_memo(g.fallback_cpu_receiver, next_epoch_start_time) );

		auto next_epoch_itr = epochs_t.find(next_epoch_start_time);

		if (next_epoch_itr == epochs_t.end()) {
			create_epoch( g, next_epoch_start_time, next_cpu_contract, g.wax_available_for_rentals );
		} else {

			asset current_wax_bucket = next_epoch_itr->wax_bucket;
			current_wax_bucket += g.wax_available_for_rentals;
			epochs_t.modify(next_epoch_itr, get_self(), [&](auto & _e) {
				_e.wax_bucket = current_wax_bucket;
			});

		}

		g.wax_available_for_rentals = ZERO_WAX;
	}

	g.next_stakeall_time += g.seconds_between_stakeall;
	global_s.set(g, _self);
}

/**
* sync
* this only exists to keep data refreshed and make it easier for front ends to display fresh data
* it's not necessary for the dapp to function properly
* therefore it requires admin auth to avoid random people spamming the network and running this constantly
* also, the logic in this action gets executed any time a user interacts with this contract, so the only
* reason to call this action is if there hasn't been a contract interaction from users
*/

ACTION fusion::sync(const eosio::name& caller) {

	require_auth( caller );

	global g = global_s.get();

	check( is_an_admin( g, caller ), ( caller.to_string() + " is not an admin" ).c_str() );

	sync_epoch( g );

	global_s.set(g, _self);
}

ACTION fusion::unstakecpu(const uint64_t& epoch_id, const int& limit) {
	
	//anyone can call this

	global g = global_s.get();

	sync_epoch( g );

	//the only epoch that should ever need unstaking is the one that started prior to current epoch
	//calculate the epoch prior to the most recently started one
	//this can be overridden by specifying an epoch_id in the action instead of passing 0
	uint64_t epoch_to_check = epoch_id == 0 ? g.last_epoch_start_time - g.seconds_between_epochs : epoch_id;

	auto epoch_itr = epochs_t.require_find( epoch_to_check, ("could not find epoch " + std::to_string( epoch_to_check ) ).c_str() );
	check( epoch_itr->time_to_unstake <= now(), ("can not unstake until another " + std::to_string( epoch_itr-> time_to_unstake - now() ) + " seconds has passed").c_str() );

	del_bandwidth_table del_tbl( SYSTEM_CONTRACT, epoch_itr->cpu_wallet.value );

	if ( del_tbl.begin() == del_tbl.end() ) {
		check( false, ( epoch_itr->cpu_wallet.to_string() + " has nothing to unstake" ).c_str() );
	}

	//the deatult amount of rows to erase is 500, can be overridden by passing a number > 0 to the action
	int rows_limit = limit == 0 ? 500 : limit;

	action(permission_level{get_self(), "active"_n}, epoch_itr->cpu_wallet, "unstakebatch"_n, std::tuple{ rows_limit }).send();

	global_s.set(g, _self);

	renters_table renters_t = renters_table( _self, epoch_to_check );
	if ( renters_t.begin() == renters_t.end() ) return;

	int count = 0;
	auto rental_itr = renters_t.begin();
	while (rental_itr != renters_t.end()) {
		if (count == rows_limit) return;
		rental_itr = renters_t.erase( rental_itr );
		count ++;
	}

}

ACTION fusion::updatetop21() {
	top21 t = top21_s.get();

	check( t.last_update + (60 * 60 * 24) <= now(), "hasn't been 24h since last top21 update" );

	auto idx = _producers.get_index<"prototalvote"_n>();

	using value_type = std::pair<eosio::producer_authority, uint16_t>;
	std::vector< value_type > top_producers;
	top_producers.reserve(21);

	for ( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
		top_producers.emplace_back(
		eosio::producer_authority{
			.producer_name = it->owner,
			.authority     = it->get_producer_authority()
		},
		it->location
		);
	}

	if ( top_producers.size() < MINIMUM_PRODUCERS_TO_VOTE_FOR ) {
		check( false, ("attempting to vote for " + std::to_string( top_producers.size() ) + " producers but need to vote for " + std::to_string( MINIMUM_PRODUCERS_TO_VOTE_FOR ) ).c_str() );
	}

	std::sort(top_producers.begin(), top_producers.end(),
	[](const value_type & a, const value_type & b) -> bool {
		return a.first.producer_name.to_string() < b.first.producer_name.to_string();
	}
	         );

	std::vector<eosio::name> producers_to_vote_for {};

	for (auto t : top_producers) {
		producers_to_vote_for.push_back(t.first.producer_name);
	}

	t.block_producers = producers_to_vote_for;
	t.last_update = now();
	top21_s.set(t, _self);
}