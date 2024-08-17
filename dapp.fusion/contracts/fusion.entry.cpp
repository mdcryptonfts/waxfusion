#include "fusion.hpp"
#include "functions.cpp"
#include "safe.cpp"
#include "on_notify.cpp"
#include "staking.cpp"
#include "redemptions.cpp"
#include "readonly.cpp"

//contractName: fusion

/** 
 * Adds a new admin wallet to the `global` singleton
 * 
 * @param admin_to_add - the wallet address of the new admin
 * 
 * @required_auth - this contract
 */

ACTION fusion::addadmin(const name& admin_to_add) {
    require_auth(_self);
    check( is_account(admin_to_add), "admin_to_add is not a wax account" );

    global g = global_s.get();

    check( std::find( g.admin_wallets.begin(), g.admin_wallets.end(), admin_to_add ) == g.admin_wallets.end(), ( admin_to_add.to_string() + " is already an admin" ).c_str() );
    
    g.admin_wallets.push_back( admin_to_add );
    global_s.set(g, _self);
}

/** 
 * Adds a new cpu contract to the `global` singleton
 * 
 * @param contract_to_add - the wallet address of the new cpu contract
 * 
 * @required_auth - this contract
 */

ACTION fusion::addcpucntrct(const name& contract_to_add) {
    require_auth(_self);
    check( is_account(contract_to_add), "contract_to_add is not a wax account" );

    global g = global_s.get();

    check( std::find( g.cpu_contracts.begin(), g.cpu_contracts.end(), contract_to_add ) == g.cpu_contracts.end(), ( contract_to_add.to_string() + " is already a cpu contract" ).c_str() );
    g.cpu_contracts.push_back( contract_to_add );
    global_s.set(g, _self);
}

/** 
 * Allows `user` to claim their rewards
 * 
 * NOTE: This action converts the claimable WAX into sWAX,
 * then liquifies the sWAX and sends lsWAX to the user.
 * Results in new sWAX and lsWAX being minted.
 * Throws if the lsWAX output is less than `minimum_output`
 * 
 * @param user - wax address of the user claiming rewards
 * @param minimum_output - expected lsWAX amount to receive
 * 
 * @required_auth - `user`
 */

ACTION fusion::claimaslswax(const name& user, const asset& minimum_output) {

    require_auth(user);

    global  g = global_s.get();
    rewards r = rewards_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);  

    extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);

    check( minimum_output > ZERO_LSWAX, "Invalid output quantity." );
    check( minimum_output.amount < MAX_ASSET_AMOUNT, "output quantity too large" );
    check( staker.claimable_wax > ZERO_WAX, "you have no wax to claim" );

    int64_t claimable_wax_amount    = staker.claimable_wax.amount;
    int64_t converted_lsWAX_i64     = calculate_lswax_output( claimable_wax_amount, g );
    check( converted_lsWAX_i64 >= minimum_output.amount, "output would be " + asset(converted_lsWAX_i64, LSWAX_SYMBOL).to_string() + " but expected " + minimum_output.to_string() );

    staker.claimable_wax        =   ZERO_WAX;
    self_staker.swax_balance    +=  asset(claimable_wax_amount, SWAX_SYMBOL);

    modify_staker(staker);
    modify_staker(self_staker);

    r.totalSupply += uint128_t(claimable_wax_amount);

    g.liquified_swax                += asset(converted_lsWAX_i64, LSWAX_SYMBOL);
    g.swax_currently_backing_lswax  += asset(claimable_wax_amount, SWAX_SYMBOL);
    g.wax_available_for_rentals     += asset(claimable_wax_amount, WAX_SYMBOL);
    g.total_rewards_claimed         += asset(claimable_wax_amount, WAX_SYMBOL);

    rewards_s.set(r, _self);    
    global_s.set(g, _self);

    issue_swax(claimable_wax_amount);
    issue_lswax(converted_lsWAX_i64, user);

}

/**
 * Claims voting rewards from the system contract
 * 
 * NOTE: We are not claiming rewards that are owed directly to this contract.
 * We are claiming rewards owed to one of our CPU contracts, by using an 
 * inline action to tell the CPU contract to claim rewards from the system.
 * 
 * @param cpu_contract - the contract that rewards are owed to
 */

ACTION fusion::claimgbmvote(const name& cpu_contract)
{
    global g = global_s.get();
    check( is_cpu_contract(g, cpu_contract), ( cpu_contract.to_string() + " is not a cpu rental contract").c_str() );
    action(active_perm(), cpu_contract, "claimgbmvote"_n, std::tuple{}).send();
}

/**
 * Claims refunds from the system contract
 * 
 * NOTE: These refunds are not owed to this contract directly.
 * They are owed to 1 or more of our CPU contracts, and we use
 * an inline action to tell our CPU contract(s) to claim their
 * refunds.
 * 
 * Throws if there are no refunds available.
 */

ACTION fusion::claimrefunds()
{
    global  g                   = global_s.get();
    bool    refund_is_available = false;

    for (name ctrct : g.cpu_contracts) {
        refunds_table   refunds_t   = refunds_table( SYSTEM_CONTRACT, ctrct.value );
        auto            refund_itr  = refunds_t.find( ctrct.value );

        if ( refund_itr != refunds_t.end() && refund_itr->request_time + seconds(REFUND_DELAY_SEC) <= current_time_point() ) {
            action(active_perm(), ctrct, "claimrefund"_n, std::tuple{}).send();
            refund_is_available = true;
        }

    }

    check( refund_is_available, "there are no refunds to claim" );
}

/**
 * Allows a `user` to claim their staking rewards
 * 
 * @param user - wallet address of the user who is claiming.
 * 
 * @required_auth - user
 */

ACTION fusion::claimrewards(const name& user) {

    require_auth(user);

    global  g = global_s.get();
    rewards r = rewards_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);

    extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);

    check( staker.claimable_wax > ZERO_WAX, "you have no wax to claim" );

    asset claimable_wax     = staker.claimable_wax;
    staker.claimable_wax    = ZERO_WAX;

    modify_staker(staker);
    modify_staker(self_staker); 

    g.total_rewards_claimed += claimable_wax;   

    global_s.set(g, _self);
    rewards_s.set(r, _self);

    transfer_tokens( user, claimable_wax, WAX_CONTRACT, std::string("your sWAX reward claim from waxfusion.io - liquid staking protocol") );
}

/**
 * Allows a `user` to claim their staking rewards.
 * 
 * NOTE: The WAX rewards will be converted to sWAX at a 1:1 ratio,
 * resulting in new sWAX being minted.
 * 
 * @param user - the wallet address of the user who is claiming
 * 
 * @required_auth - user
 */

ACTION fusion::claimswax(const name& user) {

    require_auth(user);

    global  g = global_s.get();
    rewards r = rewards_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);

    extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);

    check( staker.claimable_wax > ZERO_WAX, "you have no wax to claim" );
    int64_t swax_amount_to_claim = staker.claimable_wax.amount;

    r.totalSupply += uint128_t(swax_amount_to_claim);

    staker.claimable_wax    =   ZERO_WAX;
    staker.swax_balance     +=  asset(swax_amount_to_claim, SWAX_SYMBOL);

    modify_staker(staker);
    modify_staker(self_staker);

    g.swax_currently_earning.amount     += swax_amount_to_claim;
    g.wax_available_for_rentals.amount  += swax_amount_to_claim;
    g.total_rewards_claimed.amount      += swax_amount_to_claim;

    rewards_s.set(r, _self);
    global_s.set(g, _self);

    issue_swax(swax_amount_to_claim);
}

/**
 * Allows a `user` to erase expired redemption requests.
 * 
 * @param user - wallet address of the user who is erasing requests
 * 
 * @required_auth - user
 */

ACTION fusion::clearexpired(const name& user) {
    require_auth(user);

    global g = global_s.get();

    sync_epoch( g );

    requests_tbl requests_t = requests_tbl(get_self(), user.value);
    check( requests_t.begin() != requests_t.end(), "there are no requests to clear" );

    uint64_t    upper_bound = g.last_epoch_start_time - g.seconds_between_epochs - 1;
    auto        itr         = requests_t.begin();

    while (itr != requests_t.end()) {
        if (itr->epoch_id >= upper_bound) break;

        itr = requests_t.erase(itr);
    }

    global_s.set(g, _self);
}

/**
 * Allows anyone to compound accrued rewards for `self_staker`
 * 
 * NOTE: lsWAX earns rewards by having this contract "stake"
 * any sWAX that currently backing lsWAX. Compounding takes place
 * by claiming rewards for this underlying sWAX, and then
 * converting the WAX rewards into new sWAX, increasing the amount
 * of sWAX that backs lsWAX. New sWAX is minted in this action.
 */

ACTION fusion::compound(){

    global  g = global_s.get();
    rewards r = rewards_s.get();

    check( g.last_compound_time + (5*60) <= now(), "must wait 5 minutes between compounds" );

    sync_epoch( g );    

    auto            self_staker_itr = staker_t.require_find( _self.value, ERR_STAKER_NOT_FOUND );
    staker_struct   self_staker     = staker_struct(*self_staker_itr);    

    extend_reward(g, r, self_staker);
    update_reward(self_staker, r);  

    int64_t amount_to_compound = self_staker.claimable_wax.amount;
    check( amount_to_compound > 0, "nothing to compound" ); 
    
    self_staker.swax_balance.amount +=  amount_to_compound;
    self_staker.claimable_wax       =   ZERO_WAX;
    modify_staker(self_staker);     

    r.totalSupply += uint128_t(amount_to_compound);

    g.swax_currently_backing_lswax.amount   +=  amount_to_compound;
    g.total_rewards_claimed.amount          +=  amount_to_compound;
    g.wax_available_for_rentals.amount      +=  amount_to_compound;
    g.last_compound_time                    =   now();

    issue_swax( amount_to_compound );

    rewards_s.set(r, _self);
    global_s.set(g, _self); 

}

/** Allows anyone to use the `incentives_bucket` to create Alcor farms */

ACTION fusion::createfarms() {

    global g = global_s.get();

    sync_epoch( g );

    check( g.last_incentive_distribution + LP_FARM_DURATION_SECONDS <= now(), "hasn't been 1 week since last farms were created");
    check( g.incentives_bucket > ZERO_LSWAX, "no lswax in the incentives_bucket" );

    // we have to know what the ID of each incentive will be on alcor's contract before submitting
    // the transaction. we can do this by fetching the last row from alcor's incentives table,
    // and then incementing its ID by 1. this is what "next_key" is for
    uint64_t    next_key                = 0;
    auto        it                      = incentives_t.end();
    int64_t     total_lswax_allocated   = 0;

    if ( incentives_t.begin() != incentives_t.end() ) {
        it --;
        next_key = it->id + 1;
    }

    for (auto lp_itr = lpfarms_t.begin(); lp_itr != lpfarms_t.end(); lp_itr++) {

        int64_t         lswax_allocation_i64    = calculate_asset_share( g.incentives_bucket.amount, lp_itr->percent_share_1e6 );
        auto            incent_itr              = incent_ids_t.find( lp_itr->poolId );
        std::string     memo;

        total_lswax_allocated = safecast::add( total_lswax_allocated, lswax_allocation_i64 );

        if( incent_itr == incent_ids_t.end() ){
            create_alcor_farm( lp_itr->poolId, lp_itr->symbol_to_incentivize, lp_itr->contract_to_incentivize, safecast::safe_cast<uint32_t>(LP_FARM_DURATION_SECONDS) );

            incent_ids_t.emplace(_self, [&](auto &_incent){
                _incent.pool_id         = lp_itr->poolId;
                _incent.incentive_id    = next_key;
            });

            memo = "incentreward#" + std::to_string( next_key );
            next_key ++;
        } else {
            memo = "incentreward#" + std::to_string( incent_itr->incentive_id );
        }

        // Only the incentive creator can deposit rewards to an Alcor farm,
        // so we can assume this is safe and someone didn't inject an inline action
        // to get these rewards added to their own farm
        transfer_tokens( ALCOR_CONTRACT, asset(lswax_allocation_i64, LSWAX_SYMBOL), TOKEN_CONTRACT, memo );
    }

    check(total_lswax_allocated <= g.incentives_bucket.amount, "overallocation of incentives_bucket");

    g.incentives_bucket.amount      -=  total_lswax_allocated;
    g.last_incentive_distribution   =   now();
    global_s.set(g, _self);

}

/**
 * Initializes the contract state.
 * 
 * NOTE: The `global` singleton, `rewards` singleton, 
 * initial epoch, and `self_staker` will all be initialized.
 * Throws if `initial_reward_pool` is less than our WAX balance.
 * The initial reward distribution is set to begin 6 hours from initialization,
 * this is to allow some users to stake and avoid lsWAX compounding too quickly.
 * It would not be ideal if 1 lsWAX was worth multiple WAX as soon as we launch.
 * 
 * @param initial_reward_pool - amount of WAX we want to give to stakers for the first 24 hours.
 * 
 * @required_auth - this contract
 */

ACTION fusion::init(const asset& initial_reward_pool){
    require_auth( _self );

    accounts wax_table = accounts( WAX_CONTRACT, _self.value );
    auto balance_itr = wax_table.require_find( WAX_SYMBOL.code().raw(), "no WAX balance found" );
    check( balance_itr->balance >= initial_reward_pool, "we don't have enough WAX to cover the initial_reward_pool" );

    check( !global_s.exists(), "global singleton already exists" );
    check( !rewards_s.exists(), "rewards singleton already exists" );

    global g{};
    g.swax_currently_earning            = ZERO_SWAX;
    g.swax_currently_backing_lswax      = ZERO_SWAX;
    g.liquified_swax                    = ZERO_LSWAX;
    g.revenue_awaiting_distribution     = ZERO_WAX;
    g.total_revenue_distributed         = initial_reward_pool;
    g.wax_for_redemption                = ZERO_WAX;
    g.last_epoch_start_time             = now();
    g.wax_available_for_rentals         = ZERO_WAX;
    g.cost_to_rent_1_wax                = asset(120000, WAX_SYMBOL); /* 0.01 WAX per day */
    g.current_cpu_contract              = "cpu1.fusion"_n;
    g.next_stakeall_time                = now() + 60 * 60 * 24;    
    g.last_incentive_distribution       = 0;
    g.incentives_bucket                 = ZERO_LSWAX;
    g.total_value_locked                = initial_reward_pool;
    g.total_shares_allocated            = 0;
    g.minimum_stake_amount              = eosio::asset(100000000, WAX_SYMBOL);
    g.last_compound_time                = now();
    g.minimum_unliquify_amount          = eosio::asset(100000000, LSWAX_SYMBOL);
    g.initial_epoch_start_time          = now();
    g.cpu_rental_epoch_length_seconds   = days_to_seconds(14); /* 14 days */
    g.seconds_between_epochs            = days_to_seconds(7); /* 7 days */
    g.user_share_1e6                    = 85 * SCALE_FACTOR_1E6;
    g.pol_share_1e6                     = 7 * SCALE_FACTOR_1E6;
    g.ecosystem_share_1e6               = 8 * SCALE_FACTOR_1E6;
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
    g.redemption_period_length_seconds  = days_to_seconds(2); /* 2 days */
    g.seconds_between_stakeall          = days_to_seconds(1); /* once per day */
    g.fallback_cpu_receiver             = "updatethings"_n;
    g.protocol_fee_1e6                  = 50000; /* 0.05% */
    g.total_rewards_claimed             = ZERO_WAX;
    global_s.set(g, _self);

    create_epoch( g, now(), "cpu1.fusion"_n, ZERO_WAX );

    staker_t.emplace(_self, [&](auto &row){
        row.wallet                  = _self;
        row.swax_balance            = ZERO_SWAX;
        row.last_update             = now();
        row.claimable_wax           = ZERO_WAX;
        row.userRewardPerTokenPaid  = 0;     
    });

    rewards r{};
    r.periodStart           = now() + (60*60*6); /* 6 hours from now */
    r.periodFinish          = now() + (60*60*6) + STAKING_FARM_DURATION;
    r.rewardRate            = mulDiv128( uint128_t(initial_reward_pool.amount), SCALE_FACTOR_1E8, uint128_t(STAKING_FARM_DURATION) );
    r.rewardsDuration       = STAKING_FARM_DURATION;
    r.lastUpdateTime        = now();
    r.rewardPerTokenStored  = 0;
    r.rewardPool            = initial_reward_pool;
    r.totalSupply           = 0;  
    r.totalRewardsPaidOut   = ZERO_WAX;
    rewards_s.set(r, _self);        
}

/**
 * Initializes the top 21 singleton. 
 * 
 * NOTE: This is the production version, the unit testing version
 * of this action must be commented out / removed when deploying
 * the contract on production networks.
 */

ACTION fusion::inittop21() {
    require_auth(get_self());

    check(!top21_s.exists(), "top21 already exists");

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

    top21 t{};
    t.block_producers = producers_to_vote_for;
    t.last_update = now();
    top21_s.set(t, _self);

}


/**
 * Initializes the top 21 singleton
 * 
 * NOTE: When running unit tests, this version needs to be used,
 * and the other version needs to be commented out. This is due to 
 * issues with the mock system contracts that are included in
 * this repo for unit tests, since they behave differently 
 * than the real system contracts.
 */

/*
ACTION fusion::inittop21() {
    require_auth(get_self());

    check(!top21_s.exists(), "top21 already exists");

    auto idx = _producers.get_index<"prototalvote"_n>();


    using value_type = std::pair<eosio::producer_authority, uint16_t>;
    std::vector< value_type > top_producers;
    top_producers.reserve(21);

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
*/


/**
 * Allows a `user` to instantly redeem their sWAX
 * 
 * NOTE: By default, sWAX redemptions are subject to the standard redemption process.
 * They are free, and subject to a waiting period. We allow users to skip this process
 * by paying a 0.05% fee on the redemption. This instant redemption process is limited
 * by the amount of WAX that is currently available for CPU rentals.
 * 
 * @param user - the WAX address of the user who is redeeming
 * @param swax_to_redeem - the amount of sWAX the user is trying to redeem
 * 
 * @required_auth - user
 */

ACTION fusion::instaredeem(const name& user, const asset& swax_to_redeem) {

    require_auth(user);

    rewards r = rewards_s.get();    
    global  g = global_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);      

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

    int64_t protocol_share  = calculate_asset_share( swax_to_redeem.amount, g.protocol_fee_1e6 );
    int64_t user_share      = safecast::sub(swax_to_redeem.amount, protocol_share);

    check( safecast::add( protocol_share, user_share ) <= swax_to_redeem.amount, "error calculating protocol fee" );

    g.wax_available_for_rentals.amount      -=  swax_to_redeem.amount;
    g.revenue_awaiting_distribution.amount  +=  protocol_share;
    g.swax_currently_earning.amount         -=  swax_to_redeem.amount;

    rewards_s.set(r, _self);
    global_s.set(g, _self);

    debit_user_redemptions_if_necessary(user, staker.swax_balance);
    retire_swax(swax_to_redeem.amount);
    transfer_tokens( user, asset( user_share, WAX_SYMBOL ), WAX_CONTRACT, std::string("your sWAX redemption from waxfusion.io - liquid staking protocol") );
}

/**
 * Allows a `user` to convert their sWAX into lsWAX
 * 
 * NOTE: New lsWAX will be minted according to the current ratio
 * of sWAX/lsWAX. The user's sWAX balance will be moved to `swax_currently_backing_lswax`,
 * where it will become a part of the allocation that gets compounded.
 * 
 * @param user - the WAX address of the user who is liquifying
 * @param quantity - the amount of sWAX they want to liquify
 * 
 * @required_auth - user
 */

ACTION fusion::liquify(const name& user, const asset& quantity) {

    require_auth(user);
    check(quantity > ZERO_SWAX, "Invalid quantity.");
    check(quantity.amount < MAX_ASSET_AMOUNT, "quantity too large");

    rewards r = rewards_s.get();
    global  g = global_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);  

    extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);

    check( staker.swax_balance >= quantity, "you are trying to liquify more than you have" );

    staker.swax_balance         -= quantity;
    self_staker.swax_balance    += quantity;

    modify_staker(staker);
    modify_staker(self_staker);

    int64_t converted_lsWAX_i64 = calculate_lswax_output(quantity.amount, g);

    g.swax_currently_earning        -= quantity;
    g.swax_currently_backing_lswax  += quantity;
    g.liquified_swax.amount         += converted_lsWAX_i64;

    issue_lswax(converted_lsWAX_i64, user);
    debit_user_redemptions_if_necessary(user, staker.swax_balance);

    rewards_s.set(r, _self);
    global_s.set(g, _self);
}

/**
 * Allows a `user` to convert their sWAX into lsWAX
 * 
 * NOTE: New lsWAX will be minted according to the current ratio
 * of sWAX/lsWAX. The user's sWAX balance will be moved to `swax_currently_backing_lswax`,
 * where it will become a part of the allocation that gets compounded.
 * Throws if the output amount is less than `minimum_output`
 * 
 * @param user - the WAX address of the user who is liquifying
 * @param quantity - the amount of sWAX they want to liquify
 * @param minimum_output - the amount of lsWAX the `user` expects to receive
 * 
 * @required_auth - user
 */

ACTION fusion::liquifyexact(const name& user, const asset& quantity, const asset& minimum_output)
{

    require_auth(user);

    check(quantity > ZERO_SWAX, "Invalid quantity.");
    check(minimum_output > ZERO_LSWAX, "Invalid output quantity.");
    check(quantity.amount < MAX_ASSET_AMOUNT, "quantity too large");
    check(minimum_output.amount < MAX_ASSET_AMOUNT, "output quantity too large");

    rewards r = rewards_s.get();
    global  g = global_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);  

    extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);

    check( staker.swax_balance >= quantity, "you are trying to liquify more than you have" );

    staker.swax_balance         -= quantity;
    self_staker.swax_balance    += quantity;

    modify_staker(staker);
    modify_staker(self_staker);

    int64_t converted_lsWAX_i64 = calculate_lswax_output(quantity.amount, g);
    check( converted_lsWAX_i64 >= minimum_output.amount, "output would be " + asset(converted_lsWAX_i64, LSWAX_SYMBOL).to_string() + " but expected " + minimum_output.to_string() );

    g.swax_currently_earning        -= quantity;
    g.swax_currently_backing_lswax  += quantity;
    g.liquified_swax.amount         += converted_lsWAX_i64;

    rewards_s.set(r, _self);
    global_s.set(g, _self);

    issue_lswax(converted_lsWAX_i64, user);
    debit_user_redemptions_if_necessary(user, staker.swax_balance);
}

/**
 * Allows anyone to move unredeemed funds from redemption pool to rental pool
 * 
 * NOTE: There is a 48 hour window where redemptions take place during each epoch.
 * If any funds were requested before that window, but not actually redeemed by the
 * end of that window, then these funds will go back into the rotation where they
 * can be used for CPU rentals, instant redemptions, etc
 */

ACTION fusion::reallocate() {

    global g = global_s.get();

    sync_epoch( g );

    check( now() > g.last_epoch_start_time + g.redemption_period_length_seconds, "redemption period has not ended yet" );
    check( g.wax_for_redemption > ZERO_WAX, "there is no wax to reallocate" );

    g.wax_available_for_rentals +=  g.wax_for_redemption;
    g.wax_for_redemption        =   ZERO_WAX;

    global_s.set(g, _self);
}

/**
 * Allows a `user` to claim the redemption funds they requested
 * 
 * @param user - wallet address of the user claiming their funds
 * 
 * @required_auth - user
 */

ACTION fusion::redeem(const name& user) {

    require_auth(user);

    rewards r = rewards_s.get();
    global  g = global_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);      

    extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);
    modify_staker(self_staker);

    uint64_t redemption_start_time  = g.last_epoch_start_time;
    uint64_t redemption_end_time    = g.last_epoch_start_time + g.redemption_period_length_seconds;
    uint64_t epoch_to_claim_from    = g.last_epoch_start_time - g.cpu_rental_epoch_length_seconds;

    check( now() < redemption_end_time,
           ( "next redemption does not start until " + std::to_string(g.last_epoch_start_time + g.seconds_between_epochs) ).c_str()
         );

    requests_tbl requests_t = requests_tbl(get_self(), user.value);
    auto req_itr = requests_t.require_find(epoch_to_claim_from, "you don't have a redemption request for the current redemption period");

    // Sanity check, this should never happen because the amounts were validated when the request was created
    check( req_itr->wax_amount_requested.amount <= staker.swax_balance.amount, "you are trying to redeem more than you have" );
    check( g.wax_for_redemption >= req_itr->wax_amount_requested, "not enough wax in the redemption pool" );

    r.totalSupply       -= uint128_t(req_itr->wax_amount_requested.amount);
    staker.swax_balance -= asset(req_itr->wax_amount_requested.amount, SWAX_SYMBOL);
    modify_staker(staker);

    g.wax_for_redemption            -= req_itr->wax_amount_requested;
    g.swax_currently_earning.amount -= req_itr->wax_amount_requested.amount;
    
    rewards_s.set(r, _self);
    global_s.set(g, _self);

    retire_swax(req_itr->wax_amount_requested.amount);
    transfer_tokens( user, req_itr->wax_amount_requested, WAX_CONTRACT, std::string("your sWAX redemption from waxfusion.io - liquid staking protocol") );

    requests_t.erase(req_itr);
}

/**
 * Removes an admin_wallet from the global singleton
 * 
 * @param admin_to_remove - the wallet to remove from the global state
 * 
 * @required_auth - this contract
 */

ACTION fusion::removeadmin(const name& admin_to_remove) {
    require_auth(_self);

    global  g   = global_s.get();
    auto    itr = std::remove(g.admin_wallets.begin(), g.admin_wallets.end(), admin_to_remove);

    check( itr != g.admin_wallets.end(), (admin_to_remove.to_string() + " is not an admin").c_str() );
    
    g.admin_wallets.erase(itr, g.admin_wallets.end());
    global_s.set(g, _self);

}

/**
 * Allows a `user` to request an sWAX redemption
 * 
 * NOTE: Since new requests overwrite previous requests, the user has to 
 * pass `true` to `accept_replacing_prev_requests` if they have any existing requests.
 * Throws if there is an existing request and they passed `false`.
 * Helper functions for handling requests are found in `redemptions.cpp`
 * 
 * @param user - wax address of the user requesting a redemption
 * @param swax_to_redeem - the amount of sWAX they want to redeem for WAX
 * @param accept_replacing_prev_requests - `bool`, whether or not they are ok with replacing existing requests
 * 
 * @required_auth - user
 */

ACTION fusion::reqredeem(const name& user, const asset& swax_to_redeem, const bool& accept_replacing_prev_requests) {

    require_auth(user);

    rewards r = rewards_s.get();
    global  g = global_s.get();

    sync_epoch( g );

    auto [staker, self_staker] = get_stakers(user);  

    extend_reward(g, r, self_staker);
    update_reward(staker, r);
    update_reward(self_staker, r);
    modify_staker(self_staker); 

    check( swax_to_redeem > ZERO_SWAX, "Must redeem a positive quantity" );
    check( swax_to_redeem.amount < MAX_ASSET_AMOUNT, "quantity too large" );

    bool    request_can_be_filled       = false;
    asset   remaining_amount_to_fill    = swax_to_redeem;

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

    remove_existing_requests( epochs_to_check, staker, accept_replacing_prev_requests );
    handle_new_request( epochs_to_check, request_can_be_filled, staker, remaining_amount_to_fill );

    if ( !request_can_be_filled ) {

        // Sanity check, this should never happen
        // If epochs can't cover the request, the only other place the WAX should be is in `wax_available_for_rentals`
        check( g.wax_available_for_rentals.amount >= remaining_amount_to_fill.amount, "Request amount is greater than amount in epochs and rental pool" );

        g.wax_available_for_rentals.amount  -= remaining_amount_to_fill.amount;
        staker.swax_balance                 -= remaining_amount_to_fill;
        transfer_tokens( user, asset( remaining_amount_to_fill.amount, WAX_SYMBOL ), WAX_CONTRACT, std::string("your redemption from waxfusion.io - liquid staking protocol") );
    }

    modify_staker(staker);
    rewards_s.set(r, _self);
    global_s.set(g, _self);
}

/**
 * Removes a CPU contract from the global singleton
 * 
 * @param contract_to_remove - the CPU contract to remove from the global state
 * 
 * @required_auth - this contract
 */

ACTION fusion::rmvcpucntrct(const name& contract_to_remove) {
    require_auth(_self);

    global  g   = global_s.get();
    auto    itr = std::remove(g.cpu_contracts.begin(), g.cpu_contracts.end(), contract_to_remove);

    check( itr != g.cpu_contracts.end(), (contract_to_remove.to_string() + " is not a cpu contract").c_str() );

    g.cpu_contracts.erase(itr, g.cpu_contracts.end());
    global_s.set(g, _self);
}

/**
 * Removes an LP incentive from the `lp_farms` table
 * 
 * @param poolId - the poolId of the liquidity pair, in Alcor's `pools` table
 * 
 * @required_auth - this contract
 */

ACTION fusion::rmvincentive(const uint64_t& poolId) {
    require_auth( _self );

    global  g       = global_s.get();
    auto    lp_itr  = lpfarms_t.require_find( poolId, "this poolId doesn't exist in the lpfarms table" );

    g.total_shares_allocated = safecast::sub( g.total_shares_allocated, lp_itr->percent_share_1e6 );

    lpfarms_t.erase( lp_itr );
    global_s.set(g, _self);
}

/**
 * Changes the `fallback_cpu_receiver` in the global singleton
 * 
 * NOTE: Every 24 hours, unrented WAX is staked to the fallback receiver
 * in order to maximize APR for sWAX and lsWAX
 * 
 * @param caller - the wallet that is submitting this transaction
 * @param receiver - the new `fallback_cpu_receiver`
 * 
 * @required_auth - any admin in the global singleton
 */

ACTION fusion::setfallback(const name& caller, const name& receiver) {
    require_auth(caller);

    global g = global_s.get();

    check( is_an_admin(g, caller), "this action requires auth from one of the admin_wallets in the global table" );
    check( is_account(receiver), "cpu receiver is not a wax account" );

    g.fallback_cpu_receiver = receiver;
    global_s.set(g, _self);
}

/**
 * Adds or modifies an LP pair to the `lp_farms` table
 * 
 * NOTE: Our ecosystem fund allocates a portion of protocol revenue to 
 * creating Alcor incentives for certain lsWAX pairs. This action allows
 * us to specify which pairs get those incentives.
 * 
 * @param poolId - the poolID of the pair to incentivize, in Alcor's `pools` table
 * @param symbol_to_incentivize - the `symbol` of the token that is paired against lsWAX
 * @param contract_to_incentivize - the `contract` of the token that is paired against lsWAX
 * @param percent_share_1e6 - the percent of the ecosystem fund to allocate to this pair, scaled by 1e6
 * 
 * @required_auth - this contract
 */

ACTION fusion::setincentive(const uint64_t& poolId, const eosio::symbol& symbol_to_incentivize, const eosio::name& contract_to_incentivize, const uint64_t& percent_share_1e6) {
    require_auth( _self );
    check(percent_share_1e6 > 0, "percent_share_1e6 must be positive");

    global  g   = global_s.get();
    auto    itr = pools_t.require_find(poolId, "this poolId does not exist");

    if( itr->tokenA.quantity.symbol == symbol_to_incentivize && itr->tokenA.contract == contract_to_incentivize ){
        check( itr->tokenB.quantity.symbol == LSWAX_SYMBOL && itr->tokenB.contract == TOKEN_CONTRACT, "tokenB should be lsWAX" );
    } else if( itr->tokenB.quantity.symbol == symbol_to_incentivize && itr->tokenB.contract == contract_to_incentivize ){
        check( itr->tokenA.quantity.symbol == LSWAX_SYMBOL && itr->tokenA.contract == TOKEN_CONTRACT, "tokenA should be lsWAX" );
    } else {
        check( false, "this poolId does not contain the symbol/contract combo you entered" );
    }

    auto lp_itr = lpfarms_t.find( poolId );

    if (lp_itr == lpfarms_t.end()) {

        g.total_shares_allocated = safecast::add( g.total_shares_allocated, percent_share_1e6 );

        lpfarms_t.emplace(_self, [&](auto & _lp) {
            _lp.poolId                  = poolId;
            _lp.symbol_to_incentivize   = symbol_to_incentivize;
            _lp.contract_to_incentivize = contract_to_incentivize;
            _lp.percent_share_1e6       = percent_share_1e6;
        });

    } else {

        check( lp_itr->percent_share_1e6 != percent_share_1e6, "the share you entered is the same as the existing share" );

        if ( lp_itr->percent_share_1e6 > percent_share_1e6 ) {
            uint64_t difference         = safecast::sub( lp_itr->percent_share_1e6, percent_share_1e6 );
            g.total_shares_allocated    = safecast::sub( g.total_shares_allocated, difference );
        } else {
            uint64_t difference         = safecast::sub( percent_share_1e6, lp_itr->percent_share_1e6 );
            g.total_shares_allocated    = safecast::add( g.total_shares_allocated, difference );
        }

        lpfarms_t.modify(lp_itr, _self, [&](auto & _lp) {
            _lp.poolId                  = poolId;
            _lp.symbol_to_incentivize   = symbol_to_incentivize;
            _lp.contract_to_incentivize = contract_to_incentivize;
            _lp.percent_share_1e6       = percent_share_1e6;
        });

    }

    check( g.total_shares_allocated <= ONE_HUNDRED_PERCENT_1E6, "total shares can not be > 100%" );
    global_s.set( g, _self );

}


/**
 * Sets the percentage of revenue that goes to `pol.fusion`
 * 
 * NOTE: Values between 5 and 10% are allowed
 * 
 * @param pol_share_1e6 - the percentage to allocate to `pol.fusion`, scaled by 1e6
 * 
 * @required_auth - this contract
 */

ACTION fusion::setpolshare(const uint64_t& pol_share_1e6) {
    require_auth( _self );
    check( pol_share_1e6 >= uint64_t(5 * SCALE_FACTOR_1E6) && pol_share_1e6 <= uint64_t(10 * SCALE_FACTOR_1E6), "acceptable range is 5-10%" );

    global g = global_s.get();
    g.pol_share_1e6 = pol_share_1e6;
    global_s.set(g, _self);
}

/**
 * Sets the instant redemption fee in the `global` singleton
 * 
 * NOTE: Values between 0 and 1% are allowed
 * 
 * @param protocol_fee_1e6 - the new instant redemption fee, scaled by 1e6
 * 
 * @required_auth - this contract
 */

ACTION fusion::setredeemfee(const uint64_t& protocol_fee_1e6) {
    require_auth( _self );
    check( protocol_fee_1e6 >= 0 && protocol_fee_1e6 <= uint64_t(SCALE_FACTOR_1E6), "acceptable range is 0-1%" );

    global g = global_s.get();
    g.protocol_fee_1e6 = protocol_fee_1e6;
    global_s.set(g, _self);
}

/**
 * Sets the CPU rental price in the `global` singleton
 * 
 * NOTE: This also calls an inline action to set the CPU rental
 * price on the `pol.fusion` contract
 * 
 * @param caller - the wax address of the wallet calling this action
 * @param cost_to_rent_1_wax - the amount of WAX that users will pay for renting 1 WAX for 1 day
 * 
 * @required_auth - any admin in the global singleton
 */

ACTION fusion::setrentprice(const name& caller, const asset& cost_to_rent_1_wax) {
    require_auth(caller);

    global g = global_s.get();

    check( is_an_admin(g, caller), "this action requires auth from one of the admin_wallets in the config table" );
    check( cost_to_rent_1_wax > ZERO_WAX, "cost must be positive" );

    g.cost_to_rent_1_wax = cost_to_rent_1_wax;
    global_s.set(g, _self);

    action(active_perm(), POL_CONTRACT, "setrentprice"_n, std::tuple{ cost_to_rent_1_wax }).send();
}

ACTION fusion::setversion(const name& caller, const std::string& version_id, const std::string& changelog_url){
    require_auth(caller);

    global  g = global_s.get();
    version v = version_s.get_or_create(_self, version{});

    check( is_an_admin(g, caller), "this action requires auth from one of the admin_wallets in the config table" );

    v.version_id    = version_id;
    v.changelog_url = changelog_url;
    version_s.set(v, _self);
}

/**
 * Opens a row for `user` in the `stakers` table
 * 
 * NOTE: If the user already has a row, this action will
 * sync their data and modify the row.
 * 
 * @param user - the wax address of the user calling this action
 * 
 * @required_auth - user
 */

ACTION fusion::stake(const name& user) {

    require_auth(user);

    rewards r = rewards_s.get();
    global  g = global_s.get();

    sync_epoch( g );

    auto            staker_itr      = staker_t.find(user.value);
    staker_struct   staker          = staker_itr != staker_t.end() ? staker_struct(*staker_itr) : staker_struct();
    auto            self_staker_itr = staker_t.require_find( _self.value, ERR_STAKER_NOT_FOUND );
    staker_struct   self_staker     = staker_struct(*self_staker_itr);

    extend_reward(g, r, self_staker);
    update_reward(self_staker, r);

    if (staker_itr != staker_t.end()) {
        update_reward(staker, r);
        modify_staker(staker);
    } else {
        staker_t.emplace(user, [&](auto & _s) {
            _s.wallet                   = user;
            _s.swax_balance             = ZERO_SWAX;
            _s.claimable_wax            = ZERO_WAX;
            _s.last_update              = now();
            _s.userRewardPerTokenPaid   = r.rewardPerTokenStored;
        });
    }

    modify_staker(self_staker);
    rewards_s.set(r, _self);
    global_s.set(g, _self); 
}

/**
 * Stakes any unrented WAX at the end of each day
 * 
 * NOTE: In order to keep the protocol earning as much rewards as possible,
 * unused funds need to occasionally be staked to the `fallback_cpu_receiver`.
 */

ACTION fusion::stakeallcpu() {
    
    global g = global_s.get();

    sync_epoch( g );

    check( now() >= g.next_stakeall_time, ( "next stakeall time is not until " + std::to_string(g.next_stakeall_time) ).c_str() );

    if (g.wax_available_for_rentals.amount > 0) {

        name        next_cpu_contract       = get_next_cpu_contract( g );
        uint64_t    next_epoch_start_time   = g.last_epoch_start_time + g.seconds_between_epochs;
        auto        next_epoch_itr          = epochs_t.find(next_epoch_start_time);

        transfer_tokens( next_cpu_contract, g.wax_available_for_rentals, WAX_CONTRACT, cpu_stake_memo(g.fallback_cpu_receiver, next_epoch_start_time) );

        if (next_epoch_itr == epochs_t.end()) {
            create_epoch( g, next_epoch_start_time, next_cpu_contract, g.wax_available_for_rentals );
        } else {
            epochs_t.modify(next_epoch_itr, get_self(), [&](auto & _e) {
                _e.wax_bucket += g.wax_available_for_rentals;
            });
        }

        g.wax_available_for_rentals = ZERO_WAX;
    }

    uint64_t periods_passed = ( now() - g.next_stakeall_time + g.seconds_between_stakeall - 1 ) / g.seconds_between_stakeall;
    g.next_stakeall_time += ( periods_passed * g.seconds_between_stakeall );
    global_s.set(g, _self);
}

/**
 * Creates the current epoch if it hasn't been created yet
 * 
 * NOTE: This action is not necessary for the contract to function properly.
 * It's only used for providing front ends with up to date information, so
 * they can display things properly. Therefore, it requires admin auth to 
 * avoid unncessary spamming of transactions.
 * 
 * @param caller - the wallet address submitting this transaction
 * 
 * @required_auth - any admin in the global singleton
 */

ACTION fusion::sync(const name& caller) {

    require_auth( caller );

    global g = global_s.get();

    check( is_an_admin( g, caller ), ( caller.to_string() + " is not an admin" ).c_str() );

    sync_epoch( g );

    global_s.set(g, _self);
}

/**
 * Unstakes WAX from accounts that are being rented to
 * 
 * NOTE: We are not unstaking WAX staked by this contract directly.
 * We are notifying one of our CPU contracts about the need to unstake.
 * 
 * @param epoch_id - which epoch to unstake from. Pass `0` for default value
 * @param limit - max amount of accounts to unstake from. Pass `0` for default (500)
 */

ACTION fusion::unstakecpu(const uint64_t& epoch_id, const int& limit) {
    
    global g = global_s.get();

    sync_epoch( g );

    uint64_t    epoch_to_check  = epoch_id == 0 ? g.last_epoch_start_time - g.seconds_between_epochs : epoch_id;
    auto        epoch_itr       = epochs_t.require_find( epoch_to_check, ("could not find epoch " + std::to_string( epoch_to_check ) ).c_str() );
    int         rows_limit      = limit == 0 ? 500 : limit;

    check( epoch_itr->time_to_unstake <= now(), ("can not unstake until another " + std::to_string( epoch_itr-> time_to_unstake - now() ) + " seconds has passed").c_str() );

    del_bandwidth_table del_tbl( SYSTEM_CONTRACT, epoch_itr->cpu_wallet.value );

    if ( del_tbl.begin() == del_tbl.end() ) {
        check( false, ( epoch_itr->cpu_wallet.to_string() + " has nothing to unstake" ).c_str() );
    }

    action(active_perm(), epoch_itr->cpu_wallet, "unstakebatch"_n, std::tuple{ rows_limit }).send();

    global_s.set(g, _self);

    renters_table   renters_t   = renters_table( _self, epoch_to_check );
    auto            rental_itr  = renters_t.begin();
    int             count       = 0;

    if ( renters_t.begin() == renters_t.end() ) return;

    while (rental_itr != renters_t.end()) {
        if (count == rows_limit) return;
        rental_itr = renters_t.erase( rental_itr );
        count ++;
    }

}

/** Updates the list of block producers in the `top21` singleton */

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

    t.block_producers   = producers_to_vote_for;
    t.last_update       = now();
    top21_s.set(t, _self);
}