#pragma once

#include "polcontract.hpp"
#include "functions.cpp"
#include "on_notify.cpp"
#include "safe.cpp"

//contractName: polcontract


/** Claims voting rewards from the system contract if they are available */

ACTION polcontract::claimgbmvote()
{
    update_state();
    action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"claimgbmvote"_n,std::tuple{ get_self() }).send();
}

/** Claims refunds from the system contract if they are available */

ACTION polcontract::claimrefund()
{
    update_state();
    action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"refund"_n,std::tuple{ get_self() }).send();
}

/**
 * Unstakes and erases any CPU rentals that are expired
 * 
 * NOTE: Since we may need to unstake large batches in multiple transactions
 * (due to transaction size limits), but we also don't want to reset the refund
 * timer if a refund has been pending for a long time, this action will throw if
 * there is an existing refund that is > 5 minutes old.
 * 
 * @param limit - the maximum amount of rows to unstake/erase
 */

ACTION polcontract::clearexpired(const int& limit)
{
    update_state();
    state3 s = state_s_3.get();

    auto refund_itr = refunds_t.find( get_self().value );

    if(refund_itr != refunds_t.end()){
        bool shouldContinue = false;

        if( now() - (5 * 60) < uint64_t(refund_itr->request_time.sec_since_epoch()) ){
            shouldContinue = true;
        }

        else if( now() - days_to_seconds(3) > uint64_t(refund_itr->request_time.sec_since_epoch()) ){
            action(permission_level{get_self(), "active"_n}, SYSTEM_CONTRACT,"refund"_n,std::tuple{ get_self() }).send();
            shouldContinue = true;
        }

        check( shouldContinue, "there is a pending refund > 5 mins and < 3 days old" );
    }

    // Lower bound of 1 because we don't want to pay for deletion of unpaid rentals,
    // which are set to expire at 0 by default
    auto    expires_idx     = renters_t.get_index<"expires"_n>();
    auto    expires_lower   = expires_idx.lower_bound( 1 );
    auto    expires_upper   = expires_idx.upper_bound( now() );
    auto    itr             = expires_lower;
    int     count           = 0;

    while (itr != expires_upper) {
        if( count == limit ) break;
        if( itr->expires < now() && itr->expires != 0 ){

            s.wax_allocated_to_rentals  -= itr->amount_staked;
            s.pending_refunds           += itr->amount_staked;

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

/**
 * Allows a `renter` to erase their CPU rental row if it was never paid for
 * 
 * @param rental_id - the primary key of the row in the `rentals` table
 * 
 * @required_auth - `renter`
 */

ACTION polcontract::deleterental(const uint64_t& rental_id){
    auto itr = renters_t.require_find( rental_id, "rental does not exist" );
    require_auth( itr->renter );
    check( itr->expires == 0, "can not delete a rental after funding it, use the clearexpired action" );
    renters_t.erase( itr );
}

/**
 * Initializes the `config2` singleton if it doesnt exist yet
 * 
 * @param lswax_pool_id - the pool ID of the lsWAX/WAX pair on Alcor
 * 
 * @required_auth - this contract
 */

ACTION polcontract::initconfig(const uint64_t& lswax_pool_id){
    require_auth( _self );

    check( !config_s_2.exists(), "config2 already exists" );

    uint64_t    rental_pool_allocation_1e6  = 14285714; //14.28% or 1/7th
    uint64_t    liquidity_allocation_1e6    = ONE_HUNDRED_PERCENT_1E6 - rental_pool_allocation_1e6;
    auto        itr                         = pools_t.require_find( lswax_pool_id, "pool does not exist on alcor" );
    
    validate_liquidity_pair( itr->tokenA, itr->tokenB );

    config2 c{};
    c.liquidity_allocation_1e6      = liquidity_allocation_1e6;
    c.rental_pool_allocation_1e6    = rental_pool_allocation_1e6;
    c.lswax_wax_pool_id             = lswax_pool_id;
    config_s_2.set(c, _self);
}

/**
 * Initializes the `state3` singleton if it doesnt exist yet
 * 
 * @required_auth - this contract
 */

ACTION polcontract::initstate3(){
    require_auth( _self );
    eosio::check(!state_s_3.exists(), "state3 already exists");

    state3 s{};
    s.wax_available_for_rentals     = ZERO_WAX;
    s.next_day_end_time             = now() + SECONDS_PER_DAY;
    s.cost_to_rent_1_wax            = asset(120000, WAX_SYMBOL); // 0.0012 WAX per day
    s.last_vote_time                = 0;
    s.wax_bucket                    = ZERO_WAX;
    s.lswax_bucket                  = ZERO_LSWAX;
    s.last_liquidity_addition_time  = now();
    s.wax_allocated_to_rentals      = ZERO_WAX;
    s.pending_refunds               = ZERO_WAX;
    s.last_rebalance_time           = 0;
    state_s_3.set(s, _self);
}

/**
 * Rebalances our WAX/lsWAX buckets if the weights are not equal
 * 
 * NOTE: Since we only add LP to Alcor if the market price is acceptable,
 * this action will also move stale funds into the CPU rental pool if it has
 * been 1 week since the last time Alcor's price was acceptable.
 * Throws if the last rebalance was < 24 hours ago.
 */

ACTION polcontract::rebalance(){
    update_state();

    state3 s                = state_s_3.get();
    config2 c               = config_s_2.get();
    dapp_tables::global ds  = dapp_state_s.get();    

    if( s.wax_bucket == ZERO_WAX && s.lswax_bucket == ZERO_LSWAX ){
        check(false, "there are no assets to rebalance");
    }

    check( s.last_rebalance_time <= now() - SECONDS_PER_DAY, "can only rebalance once every 24 hours" );

    liquidity_struct lp_details = get_liquidity_info( c, ds );

    if( !lp_details.is_in_range && s.last_liquidity_addition_time < now() - ( days_to_seconds(7) ) ){

        bool can_rebalance = false;

        if( s.wax_bucket > ZERO_WAX ){
            s.wax_available_for_rentals +=  s.wax_bucket;
            s.wax_bucket                =   ZERO_WAX;
            can_rebalance               =   true;
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

            s.wax_bucket.amount     -=  amount_to_transfer;
            s.last_rebalance_time   =   now();
            state_s_3.set(s, _self);

            transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );
            return; 

        } else if( s.lswax_bucket > ZERO_LSWAX && s.wax_bucket == ZERO_WAX ){

            check( s.lswax_bucket >= ds.minimum_unliquify_amount && ds.wax_available_for_rentals.amount > 0, "can not rebalance because we're unable to instant redeem lswax for wax" );

            int64_t max_redeemable = calculate_lswax_output( ds.wax_available_for_rentals.amount, ds );

            check( max_redeemable > ds.minimum_unliquify_amount.amount, 
                ( DAPP_CONTRACT.to_string() + " doesn't have enough wax in the instant redemption pool to rebalance " )
                .c_str() );                 

            int64_t max_output_amount   = s.lswax_bucket.amount > 0 ? calculate_swax_output( s.lswax_bucket.amount, ds ) : 0;
            int64_t max_weight          = std::min( max_output_amount, max_redeemable );

            check( max_weight > 1, "division would result in a nonpositive quantity" );

            int64_t weighted_amount_to_transfer = safecast::div( max_weight, int64_t(2) );
            int64_t amount_to_transfer          = calculate_lswax_output( weighted_amount_to_transfer, ds );

            check( amount_to_transfer > 0, "can not transfer this amount" );                

            s.lswax_bucket.amount -=    amount_to_transfer;
            s.last_rebalance_time =     now();
            state_s_3.set(s, _self);

            transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("rebalance") );
            return;             

        } else {

            int64_t weighted_lswax_bucket   = calculate_swax_output( s.lswax_bucket.amount, ds );
            int64_t total_weight            = safecast::add( weighted_lswax_bucket, s.wax_bucket.amount );
            int64_t half_weight             = safecast::div( total_weight, int64_t(2) );

            if( half_weight > weighted_lswax_bucket ){

                int64_t amount_to_transfer = safecast::sub( half_weight, weighted_lswax_bucket );
                check( amount_to_transfer >= 500000000, "amount to rebalance is too small" );

                s.wax_bucket.amount     -= amount_to_transfer;
                s.last_rebalance_time   = now();
                state_s_3.set(s, _self);

                transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );
                return;                                 

            } else if( half_weight > s.wax_bucket.amount ){

                int64_t weight_difference   = safecast::sub( half_weight, s.wax_bucket.amount );
                int64_t difference_adjusted = calculate_lswax_output( weight_difference, ds );

                int64_t max_redeemable = 
                    ds.wax_available_for_rentals.amount > 0 ?
                        calculate_lswax_output( ds.wax_available_for_rentals.amount, ds )
                    : 0;

                int64_t amount_to_transfer = std::min( difference_adjusted, max_redeemable );
                check( amount_to_transfer >= 500000000, "amount to rebalance is too small" );

                s.lswax_bucket.amount -=    amount_to_transfer;
                s.last_rebalance_time =     now();
                state_s_3.set(s, _self);

                transfer_tokens( DAPP_CONTRACT, asset( amount_to_transfer, LSWAX_SYMBOL), TOKEN_CONTRACT, std::string("rebalance") );
                return;    

            }
        }

    } else {
        check( false, "no need to rebalance" ); 
    }
}

/**
 * Opens a row for a CPU rental if it doesn't exist yet
 * 
 * @param renter - the WAX address paying for the rental
 * @param cpu_receiver - the WAX address to rent the CPU to
 * 
 * @required_auth - renter
 */

ACTION polcontract::rentcpu(const name& renter, const name& cpu_receiver){
    require_auth(renter);
    update_state();

    check( is_account(cpu_receiver), (cpu_receiver.to_string() + " is not a valid account").c_str() );

    auto            renter_receiver_idx     = renters_t.get_index<"fromtocombo"_n>();
    const uint128_t renter_receiver_combo   = mix64to128(renter.value, cpu_receiver.value);
    auto            itr                     = renter_receiver_idx.find(renter_receiver_combo); 

    if(itr == renter_receiver_idx.end()){
        renters_t.emplace(renter, [&](auto &_r){
            _r.ID               = renters_t.available_primary_key();
            _r.renter           = renter;
            _r.rent_to_account  = cpu_receiver;
            _r.amount_staked    = ZERO_WAX;
            _r.expires          = 0;         
        });
    }
}

/**
 * Adjusts the percentage of revenue that goes to providing liquidity on Alcor
 * 
 * Throws if the value is < 1% or > 100%
 * 
 * @param liquidity_allocation_percent_1e6 - the percentage to allocate to liquidity, scaled by 1e6
 * 
 * @required_auth - this contract
 */

ACTION polcontract::setallocs(const uint64_t& liquidity_allocation_percent_1e6){
    require_auth( _self );

    check( liquidity_allocation_percent_1e6 >= 1000000 && liquidity_allocation_percent_1e6 <= ONE_HUNDRED_PERCENT_1E6, "percent must be between > 1e6 && <= 100 * 1e6" );

    config2 c = config_s_2.get();
    c.liquidity_allocation_1e6      = liquidity_allocation_percent_1e6;
    c.rental_pool_allocation_1e6    = ONE_HUNDRED_PERCENT_1E6 - liquidity_allocation_percent_1e6;
    config_s_2.set(c, _self);
}

/**
 * Adjusts the price for renting CPU from this contract
 * 
 * NOTE: This action is called inline when setting the rent price
 * on the dapp.fusion contract
 * 
 * @param cost_to_rent_1_wax - the WAX amount that a user will pay to rent 1 WAX for 1 day
 * 
 * @required_auth - dapp.fusion
 */

ACTION polcontract::setrentprice(const asset& cost_to_rent_1_wax){
    require_auth( DAPP_CONTRACT );
    update_state();

    check( cost_to_rent_1_wax > ZERO_WAX, "cost must be positive" );

    state3 s = state_s_3.get();
    s.cost_to_rent_1_wax = cost_to_rent_1_wax;
    state_s_3.set(s, _self);
}