#pragma once

/**
 *  for unit testing
 *  since the eosio contract in our local environment doesnt automatically adjust our wax 
 *  balance when we stake CPU, I just added a require_recipient in the delegatebw action.
 *  then we get a notification requesting the payment within the same transaction
 *  and inline transfer wax to eosio, mimicing the behaviour of the real system contract
 * 
 *  must be commented out or removed in production
 */

/*
void polcontract::receive_system_request(const name& payer, const asset& wax_amount){
    if(payer == _self){
        transfer_tokens( "eosio"_n, wax_amount, WAX_CONTRACT, "stake" );
    }
}
*/

void polcontract::receive_wax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
    if( quantity.amount == 0 || from == _self || to != _self ) return;

    const name tkcontract = get_first_receiver();

    check( quantity.amount > 0, "Must send a positive quantity" );
    check( quantity.amount < MAX_ASSET_AMOUNT, "quantity too large" );
    check( quantity.symbol == WAX_SYMBOL, "was expecting WAX token" );

    update_state();

    state3 s                = state_s_3.get();
    config2 c               = config_s_2.get();
    dapp_tables::global ds  = dapp_state_s.get();    

    if( memo == "pol allocation from waxfusion distribution" ){
        check( from == DAPP_CONTRACT, "invalid sender for this memo" );

        int64_t liquidity_allocation    = calculate_asset_share( quantity.amount, c.liquidity_allocation_1e6 );
        int64_t rental_pool_allocation  = calculate_asset_share( quantity.amount, c.rental_pool_allocation_1e6 );
        int64_t wax_bucket_allocation   = 0;
        int64_t buy_lswax_allocation    = 0;        

        liquidity_struct lp_details = get_liquidity_info( c, ds );

        if( lp_details.is_in_range ){
            calculate_liquidity_allocations( lp_details, liquidity_allocation, wax_bucket_allocation, buy_lswax_allocation );
            transfer_tokens( DAPP_CONTRACT, asset( buy_lswax_allocation, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );
        } else {
            wax_bucket_allocation = liquidity_allocation;
        }

        validate_allocations( quantity.amount, {buy_lswax_allocation, wax_bucket_allocation, rental_pool_allocation} );

        s.wax_available_for_rentals.amount  = safecast::add( s.wax_available_for_rentals.amount, rental_pool_allocation );
        s.wax_bucket.amount                 = safecast::add( s.wax_bucket.amount, wax_bucket_allocation );
        state_s_3.set(s, _self);

        return;
    }

    if( memo == "voter pay" ){
        check( from == "eosio.voters"_n, "voter pay must come from eosio.voters" );
        transfer_tokens( DAPP_CONTRACT, quantity, WAX_CONTRACT, std::string("waxfusion_revenue") );
        return;
    }

    //staked CPU refund was processed and being returned
    if( memo == "unstake" ){
        check( from == "eosio.stake"_n, "unstakes should come from eosio.stake" );

        s.wax_available_for_rentals += quantity;
        s.pending_refunds           -= quantity;
        state_s_3.set(s, _self);

        return;     
    }

    if( memo == "rebalance" ){

        check( from == DAPP_CONTRACT, ("only " + DAPP_CONTRACT.to_string() + " should send with this memo" ).c_str() );

        s.wax_bucket += quantity;

        liquidity_struct lp_details = get_liquidity_info( c, ds );

        if( lp_details.is_in_range && s.lswax_bucket > ZERO_LSWAX ){
            add_liquidity( s, lp_details );
            s.last_liquidity_addition_time = now();
        }

        state_s_3.set(s, _self);
        return;         
    }

    if( memo == "for liquidity only" ){

        int64_t liquidity_allocation    = quantity.amount;
        int64_t wax_bucket_allocation   = 0;
        int64_t buy_lswax_allocation    = 0;        

        liquidity_struct lp_details = get_liquidity_info( c, ds );

        if( lp_details.is_in_range ){
            calculate_liquidity_allocations( lp_details, liquidity_allocation, wax_bucket_allocation, buy_lswax_allocation );
            transfer_tokens( DAPP_CONTRACT, asset( buy_lswax_allocation, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );
        } else {
            wax_bucket_allocation = liquidity_allocation;
        }

        validate_allocations( quantity.amount, {buy_lswax_allocation, wax_bucket_allocation} );

        s.wax_bucket.amount += wax_bucket_allocation;
        state_s_3.set(s, _self);

        return;           
    }

    if( memo == "for staking pool only" ){

        s.wax_available_for_rentals += quantity;
        state_s_3.set(s, _self);

        return;           
    }    

    std::vector<std::string> words = parse_memo( memo );

    if( words[1] == "rent_cpu" ){
        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( words.size() >= 5, "memo for rent_cpu operation is incomplete" );

        int64_t         profit_made                 = quantity.amount;
        const name      cpu_receiver                = eosio::name( words[2] );
        auto            renter_receiver_idx         = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo       = mix64to128(from.value, cpu_receiver.value);
        auto            itr                         = renter_receiver_idx.require_find(renter_receiver_combo, "you need to use the rentcpu action first");  
        const uint64_t  days_to_rent                = std::strtoull( words[3].c_str(), NULL, 0 );
        const uint64_t  whole_number_of_wax_to_rent = std::strtoull( words[4].c_str(), NULL, 0 );
        const uint64_t  wax_amount_to_rent          = safecast::mul( whole_number_of_wax_to_rent, uint64_t(SCALE_FACTOR_1E8) );
        int64_t         amount_expected             = cpu_rental_price( days_to_rent, s.cost_to_rent_1_wax.amount, int64_t(wax_amount_to_rent) );

        check( days_to_rent >= MINIMUM_CPU_RENTAL_DAYS, ( "minimum days to rent is " + std::to_string( MINIMUM_CPU_RENTAL_DAYS ) ).c_str() );
        check( days_to_rent <= MAXIMUM_CPU_RENTAL_DAYS, ( "maximum days to rent is " + std::to_string( MAXIMUM_CPU_RENTAL_DAYS ) ).c_str() );        
        check( whole_number_of_wax_to_rent >= safecast::div( MINIMUM_WAX_TO_RENT, uint64_t(SCALE_FACTOR_1E8) ), ("minimum wax amount to rent is " + asset( int64_t(MINIMUM_WAX_TO_RENT), WAX_SYMBOL ).to_string() ).c_str() );
        check( whole_number_of_wax_to_rent <= safecast::div( MAXIMUM_WAX_TO_RENT, uint64_t(SCALE_FACTOR_1E8) ), ("maximum wax amount to rent is " + asset( int64_t(MAXIMUM_WAX_TO_RENT), WAX_SYMBOL ).to_string() ).c_str() );
        check( itr->expires == 0 && itr->amount_staked.amount == 0, "memo for increasing/extending should start with extend_rental or increase_rental" );
        check( s.wax_available_for_rentals.amount >= int64_t(wax_amount_to_rent), "there is not enough wax in the rental pool to cover this rental" );
        check( quantity.amount >= amount_expected, ("expected to receive " + asset( amount_expected, WAX_SYMBOL ).to_string() ).c_str() );

        s.wax_available_for_rentals.amount  -= int64_t(wax_amount_to_rent);
        s.wax_allocated_to_rentals.amount   += int64_t(wax_amount_to_rent);

        issue_refund_if_user_overpaid( from, quantity, amount_expected, profit_made );       
        stake_wax( cpu_receiver, int64_t(wax_amount_to_rent), 0 );

        renter_receiver_idx.modify(itr, same_payer, [&](auto &_r){
            _r.amount_staked.amount = int64_t(wax_amount_to_rent);
            _r.expires              = s.next_day_end_time + days_to_seconds( days_to_rent );
        });

        check( profit_made > 0, "error with rental cost calculation" );

        state_s_3.set(s, _self);

        transfer_tokens( DAPP_CONTRACT, asset( profit_made, WAX_SYMBOL ), WAX_CONTRACT, "waxfusion_revenue" );

        update_votes();
        return;
    }    

    if( words[1] == "extend_rental" ){
        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( words.size() >= 4, "memo for extend_rental operation is incomplete" );

        int64_t         profit_made             = quantity.amount;
        const name      cpu_receiver            = eosio::name( words[2] );
        auto            renter_receiver_idx     = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo   = mix64to128(from.value, cpu_receiver.value);
        auto            itr                     = renter_receiver_idx.require_find(renter_receiver_combo, "could not locate an existing rental for this renter/receiver combo");  
        const uint64_t  days_to_rent            = std::strtoull( words[3].c_str(), NULL, 0 );
        const int64_t   wax_amount_to_rent      = itr->amount_staked.amount;
        int64_t         amount_expected         = cpu_rental_price( days_to_rent, s.cost_to_rent_1_wax.amount, wax_amount_to_rent );

        check( days_to_rent >= 1, "extension must be at least 1 day" );
        check( days_to_rent <= MAXIMUM_CPU_RENTAL_DAYS, ( "maximum days to rent is " + std::to_string( MAXIMUM_CPU_RENTAL_DAYS ) ).c_str() );
        check( itr->expires != 0, "you can't extend a rental if it hasnt been funded yet" );
        check( itr->expires > now(), "you can't extend a rental after it expired" );
        check( quantity.amount >= amount_expected, ("expected to receive " + asset( amount_expected, WAX_SYMBOL ).to_string() ).c_str() );

        issue_refund_if_user_overpaid( from, quantity, amount_expected, profit_made );       

        renter_receiver_idx.modify(itr, same_payer, [&](auto &_r){
            _r.expires += days_to_seconds( days_to_rent );
        });

        check( profit_made > 0, "error with rental cost calculation" );

        state_s_3.set(s, _self);

        transfer_tokens( DAPP_CONTRACT, asset( profit_made, WAX_SYMBOL ), WAX_CONTRACT, "waxfusion_revenue" );        

        update_votes();
        return;
    } 

    if( words[1] == "increase_rental" ){
        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( words.size() >= 4, "memo for increase_rental operation is incomplete" );
        
        int64_t         profit_made                 = quantity.amount;
        const name      cpu_receiver                = eosio::name( words[2] );
        auto            renter_receiver_idx         = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo       = mix64to128(from.value, cpu_receiver.value);
        auto            itr                         = renter_receiver_idx.require_find(renter_receiver_combo, "could not locate an existing rental for this renter/receiver combo");  
        int64_t         existing_rental_amount      = itr->amount_staked.amount;
        const uint64_t  whole_number_of_wax_to_rent = std::strtoull( words[3].c_str(), NULL, 0 );
        const uint64_t  wax_amount_to_rent          = safecast::mul( whole_number_of_wax_to_rent, uint64_t(SCALE_FACTOR_1E8) );
        const int64_t   combined_wax_amount         = safecast::add( int64_t(wax_amount_to_rent), int64_t(existing_rental_amount) );
        uint64_t        seconds_remaining           = itr->expires - now();
        int64_t         amount_expected             = cpu_rental_price_from_seconds( seconds_remaining, s.cost_to_rent_1_wax.amount, wax_amount_to_rent );

        check( whole_number_of_wax_to_rent >= safecast::div( MINIMUM_WAX_TO_INCREASE, uint64_t(SCALE_FACTOR_1E8) ), ("minimum wax amount to increase is " + asset( int64_t(MINIMUM_WAX_TO_INCREASE), WAX_SYMBOL ).to_string() ).c_str() );
        check( combined_wax_amount <= int64_t(MAXIMUM_WAX_TO_RENT), ( "maximum wax amount to rent is " + asset( int64_t(MAXIMUM_WAX_TO_RENT), WAX_SYMBOL ).to_string() ).c_str() );
        check( itr->expires != 0, "you can't increase a rental if it hasnt been funded yet" );
        check( itr->expires > now(), "this rental has already expired" );
        check( seconds_remaining >= SECONDS_PER_DAY, "cant increase rental with < 1 day remaining" );
        check( s.wax_available_for_rentals.amount >= int64_t(wax_amount_to_rent), "there is not enough wax in the rental pool to cover this rental" );

        s.wax_available_for_rentals.amount  -= int64_t(wax_amount_to_rent);
        s.wax_allocated_to_rentals.amount   += int64_t(wax_amount_to_rent);

        check( quantity.amount >= amount_expected, ("expected to receive " + asset( amount_expected, WAX_SYMBOL ).to_string() ).c_str() );

        issue_refund_if_user_overpaid( from, quantity, amount_expected, profit_made );
        stake_wax( cpu_receiver, int64_t(wax_amount_to_rent), 0 );

        renter_receiver_idx.modify(itr, same_payer, [&](auto &_r){
            _r.amount_staked.amount = safecast::add( _r.amount_staked.amount, int64_t(wax_amount_to_rent) );
        });

        check( profit_made > 0, "error with rental cost calculation" );

        state_s_3.set(s, _self);

        transfer_tokens( DAPP_CONTRACT, asset( profit_made, WAX_SYMBOL ), WAX_CONTRACT, "waxfusion_revenue" );

        update_votes();
        return;
    }

    // Below is handling for any cases where the memo has not been caught yet.
    // Need to be 100% sure that each case above has a return statement or else this will cause issues

    s.wax_bucket += quantity;
    state_s_3.set(s, _self);    
}

void polcontract::receive_lswax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
    if( quantity.amount == 0 || from == _self || to != _self ) return;

    const name tkcontract = get_first_receiver();

    check( quantity.amount > 0, "Must send a positive quantity" );
    check( quantity.amount < MAX_ASSET_AMOUNT, "quantity too large" );
    check( quantity.symbol == LSWAX_SYMBOL, "was expecting LSWAX token" );

    update_state();

    state3              s   = state_s_3.get();
    config2             c   = config_s_2.get();
    dapp_tables::global ds  = dapp_state_s.get();    

    if( memo == "liquidity" && ( from == DAPP_CONTRACT  /* || DEBUG */  ) ){

        s.lswax_bucket += quantity;

        liquidity_struct lp_details = get_liquidity_info( c, ds );

        if( lp_details.is_in_range && s.wax_bucket > ZERO_WAX ){
            add_liquidity( s, lp_details );
            s.last_liquidity_addition_time = now();
        }

        state_s_3.set(s, _self);
        return;
    }

    s.lswax_bucket += quantity;
    state_s_3.set(s, _self);

}