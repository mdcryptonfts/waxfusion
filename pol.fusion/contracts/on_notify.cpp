#pragma once

/**
 *  for unit testing
 *  since the eosio contract in our local environment doesnt automatically adjust our wax 
 *  balance when we stake CPU, I just added a require_recipient in the delegatebw action.
 *  then we get a notification requesting the payment within the same transaction
 *  and inline transfer wax to eosio, mimicing the behaviour of the real system contract
 */
void polcontract::receive_system_request(const name& payer, const asset& wax_amount){
    if(!DEBUG) return;

    if(payer == _self){
        transfer_tokens( "eosio"_n, wax_amount, WAX_CONTRACT, "stake" );
    }
}

void polcontract::receive_wax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
    if( quantity.amount == 0 ) return;

    const name tkcontract = get_first_receiver();

    check( quantity.amount > 0, "Must send a positive quantity" );

    check( quantity.amount < MAX_ASSET_AMOUNT, "quantity too large" );

    if( from == get_self() || to != get_self() ){
        return;
    }

    check( quantity.symbol == WAX_SYMBOL, "was expecting WAX token" );

    update_state();

    /** pol allocation memo
     *  this catches any deposits of revenue from the dapp contract
     *  it will be split up according to the config, and the current state of the chain
     *  i.e. if the lswax price on alcor is too far from the "real" price,
     *  this case will be handled differently
     */

    if( memo == "pol allocation from waxfusion distribution" ){
        check( from == DAPP_CONTRACT, "invalid sender for this memo" );

        state3 s = state_s_3.get();
        config2 c = config_s_2.get();
        dapp_tables::state ds = dapp_state_s.get();

        //use the config to determine how much goes to liquidity, and how much for cpu rental pool
        //quantity and liquidity_allocation_1e6 are already validated to be positive
        //rental_pool_allocation_1e6 could possibly be 0
        int64_t liquidity_allocation = calculate_asset_share( quantity.amount, c.liquidity_allocation_1e6 );
        int64_t rental_pool_allocation = calculate_asset_share( quantity.amount, c.rental_pool_allocation_1e6 );

        //initialize 0 buckets before calculating how much of the liquidity_allocation to wax, and how much to lswax
        int64_t wax_bucket_allocation = 0;
        int64_t buy_lswax_allocation = 0;        

        //find out what the price is on alcor, and on the dapp.fusion contract
        liquidity_struct lp_details = get_liquidity_info( c, ds );

        //if alcors price is acceptable, convert some to lswax
        if( lp_details.is_in_range ){
            calculate_liquidity_allocations( lp_details, liquidity_allocation, wax_bucket_allocation, buy_lswax_allocation );
            transfer_tokens( DAPP_CONTRACT, asset( buy_lswax_allocation, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );
        } else {
            //alcor price is out of range so we will just store the wax in our bucket instead
            //if it goes back in range later, the rebalance action on this contract will convert
            //the proper ratios according to the price at that given time point
            wax_bucket_allocation = liquidity_allocation;
        }

        //make sure all the numbers add up without overallocating the amount received
        validate_allocations( quantity.amount, {buy_lswax_allocation, wax_bucket_allocation, rental_pool_allocation} );

        //update the state singleton ( the rest of the transfer amount will be added when the lswax is returned )
        s.wax_available_for_rentals.amount = safeAddInt64( s.wax_available_for_rentals.amount, rental_pool_allocation );
        s.wax_bucket.amount = safeAddInt64( s.wax_bucket.amount, wax_bucket_allocation );
        state_s_3.set(s, _self);

        return;
    }

    //receive voting rewards and send them to the dapp contract as revenue
    if( memo == "voter pay" ){
        check( from == "eosio.voters"_n, "voter pay must come from eosio.voters" );
        transfer_tokens( DAPP_CONTRACT, quantity, WAX_CONTRACT, std::string("waxfusion_revenue") );
        return;
    }

    //staked CPU refund was processed and being returned
    if( memo == "unstake" ){
        check( from == "eosio.stake"_n, "unstakes should come from eosio.stake" );

        //update the state singleton and make the wax available for new rentals
        state3 s = state_s_3.get();
        s.wax_available_for_rentals.amount = safeAddInt64( s.wax_available_for_rentals.amount, quantity.amount );  
        s.pending_refunds.amount = safeSubInt64( s.pending_refunds.amount, quantity.amount );
        state_s_3.set(s, _self);

        return;     
    }

    //the rebalance action was called on this contract
    //LSWAX was converted to wax so we can add liquidity to alcor
    //since the weights were calculated in the rebalance action, there is no need
    //to convert any of this into LSWAX (matching LSWAX is already in the lswax_bucket)
    if( memo == "rebalance" ){
        //no one should be sending this memo besides the dapp contract
        check( from == DAPP_CONTRACT, ("only " + DAPP_CONTRACT.to_string() + " should send with this memo" ).c_str() );

        state3 s = state_s_3.get();
        config2 c = config_s_2.get();
        dapp_tables::state ds = dapp_state_s.get();

        //add the funds into the wax_bucket in the state singleton
        s.wax_bucket.amount = safeAddInt64( s.wax_bucket.amount, quantity.amount );

        //get info about the prices on alcor and the dapp contract
        liquidity_struct lp_details = get_liquidity_info( c, ds );

        //technically the rebalance action already did this check,
        //but its good practice to verify it again before we attempt to add liquidity
        if( lp_details.is_in_range && s.lswax_bucket > ZERO_LSWAX ){

            //state is passed as a reference so it can be updated in the add_liquidity action
            //before setting the state below this block
            add_liquidity( s, lp_details );
            s.last_liquidity_addition_time = now();
        }

        state_s_3.set(s, _self);
        return;         
    }

    if( memo == "for liquidity only" ){
        /** 
        * this is the case for when 100% of the funds should be used for liquidity,
        * rather than the split used for waxfusion distributions
        * i.e. when wax team makes a deposit that should be used only for alcor
        */

        state3 s = state_s_3.get();
        config2 c = config_s_2.get();
        dapp_tables::state ds = dapp_state_s.get();

        //100% of these funds are for liquidity
        int64_t liquidity_allocation = quantity.amount;

        //Initialize empty bucket variables before calculating the ratio
        int64_t wax_bucket_allocation = 0;
        int64_t buy_lswax_allocation = 0;        

        //Get relevant details about alcor's price and the dapp contract's price
        liquidity_struct lp_details = get_liquidity_info( c, ds );

        //if alcors price is acceptable
        if( lp_details.is_in_range ){
            calculate_liquidity_allocations( lp_details, liquidity_allocation, wax_bucket_allocation, buy_lswax_allocation );
            transfer_tokens( DAPP_CONTRACT, asset( buy_lswax_allocation, WAX_SYMBOL), WAX_CONTRACT, std::string("wax_lswax_liquidity") );
        } else {
            //alcor price is out of range so we will just store the wax in our bucket instead
            wax_bucket_allocation = liquidity_allocation;
        }

        //make sure the numbers add up properly
        validate_allocations( quantity.amount, {buy_lswax_allocation, wax_bucket_allocation} );

        //update the state singleton. liquidity will be added in the lswax notification handler
        s.wax_bucket.amount = safeAddInt64( s.wax_bucket.amount, wax_bucket_allocation );
        state_s_3.set(s, _self);

        return;           
    }

    if( memo == "for staking pool only" ){
        /** 
        * this is the case for when 100% of the funds should be used for cpu rentals/staking,
        * rather than using a portion for alcor liquidity
        * this memo is used when the dapp contract sends wax back after we rebalance
        * it can also be used in other cases
        */

        state3 s = state_s_3.get();
        s.wax_available_for_rentals.amount = safeAddInt64( s.wax_available_for_rentals.amount, quantity.amount );
        state_s_3.set(s, _self);

        return;           
    }    

    //anything below here should be dynamic memo that needs to be parsed
    //except for the fallback logic at the bottom
    std::vector<std::string> words = parse_memo( memo );

    //the rentcpu action was called on this contract and a row was opened for the renter/receiver combo
    //now the renter is funding their new (unpaid) CPU rental
    if( words[1] == "rent_cpu" ){
        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( words.size() >= 5, "memo for rent_cpu operation is incomplete" );

        state3 s = state_s_3.get();

        int64_t profit_made = quantity.amount;

        //memo should also include account to rent to 
        //the rentcpu action already verified that the cpu_receiver is_account(),
        //so this just needs to match an existing rental
        const eosio::name cpu_receiver = eosio::name( words[2] );

        //make sure a record for this combo already exists
        auto renter_receiver_idx = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo = mix64to128(from.value, cpu_receiver.value);

        auto itr = renter_receiver_idx.require_find(renter_receiver_combo, "you need to use the rentcpu action first");  

        //the length of the rental is not submitted in the rentcpu action
        //it should be included in this memo
        const uint64_t days_to_rent = std::strtoull( words[3].c_str(), NULL, 0 );
        check( days_to_rent >= MINIMUM_CPU_RENTAL_DAYS, ( "minimum days to rent is " + std::to_string( MINIMUM_CPU_RENTAL_DAYS ) ).c_str() );
        check( days_to_rent <= MAXIMUM_CPU_RENTAL_DAYS, ( "maximum days to rent is " + std::to_string( MAXIMUM_CPU_RENTAL_DAYS ) ).c_str() );        

        /** wax_amount_to_rent
         *  only "whole numbers" of wax can be rented
         *  therefore the memo accepts an integer that isn't yet scaled
         *  i.e. '10' in the memo == 10.00000000 WAX
         *  we need to scale by 1e8 to get the correct number to create an asset
         */
        const uint64_t whole_number_of_wax_to_rent = std::strtoull( words[4].c_str(), NULL, 0 );
        check( whole_number_of_wax_to_rent >= safeDivUInt64( MINIMUM_WAX_TO_RENT, uint64_t(SCALE_FACTOR_1E8) ), ("minimum wax amount to rent is " + asset( int64_t(MINIMUM_WAX_TO_RENT), WAX_SYMBOL ).to_string() ).c_str() );
        check( whole_number_of_wax_to_rent <= safeDivUInt64( MAXIMUM_WAX_TO_RENT, uint64_t(SCALE_FACTOR_1E8) ), ("maximum wax amount to rent is " + asset( int64_t(MAXIMUM_WAX_TO_RENT), WAX_SYMBOL ).to_string() ).c_str() );

        const uint64_t wax_amount_to_rent = safeMulUInt64( whole_number_of_wax_to_rent, uint64_t(SCALE_FACTOR_1E8) );

        //make sure this CPU rental has not already been funded
        //there are other memos for extending/increasing an existing rental
        check( itr->expires == 0 && itr->amount_staked.amount == 0, "memo for increasing/extending should start with extend_rental or increase_rental" );

        //make sure there is enough wax available for this rental
        check( s.wax_available_for_rentals.amount >= int64_t(wax_amount_to_rent), "there is not enough wax in the rental pool to cover this rental" );

        //debit the wax from the rental pool and update the amount currently in use
        s.wax_available_for_rentals.amount = safeSubInt64( s.wax_available_for_rentals.amount, int64_t(wax_amount_to_rent) );
        s.wax_allocated_to_rentals.amount = safeAddInt64( s.wax_allocated_to_rentals.amount, int64_t(wax_amount_to_rent) );

        //calculate the cost of this rental and make sure the payment is enough to cover it
        int64_t amount_expected = cpu_rental_price( days_to_rent, s.cost_to_rent_1_wax.amount, int64_t(wax_amount_to_rent) );
        check( quantity.amount >= amount_expected, ("expected to receive " + asset( amount_expected, WAX_SYMBOL ).to_string() ).c_str() );

        //if they sent more than expected, calculate difference and refund it
        if( quantity.amount > amount_expected ){
            int64_t amount_to_refund = safeSubInt64( quantity.amount, amount_expected );

            if( amount_to_refund > 0 ){
                profit_made = safeSubInt64( quantity.amount, amount_to_refund );
                transfer_tokens( from, asset( amount_to_refund, WAX_SYMBOL ), WAX_CONTRACT, "cpu rental refund from waxfusion.io - liquid staking protocol" );
            }
        }        

        //stake cpu to the receiver
        stake_wax( cpu_receiver, int64_t(wax_amount_to_rent), 0 );

        //update the renter row (amount staked and expires)
        renter_receiver_idx.modify(itr, same_payer, [&](auto &_r){
            _r.amount_staked.amount = int64_t(wax_amount_to_rent);
            _r.expires = s.next_day_end_time + days_to_seconds( days_to_rent );
        });

        //validate that the protocol made a profit
        check( profit_made > 0, "error with rental cost calculation" );

        //send the profit to the dapp contract so it can be distributed later
        transfer_tokens( DAPP_CONTRACT, asset( profit_made, WAX_SYMBOL ), WAX_CONTRACT, "waxfusion_revenue" );

        //update the state
        state_s_3.set(s, _self);

        //make sure we are voting at full strength for relevant BPs
        update_votes();
        return;
    }    

    //there is an existing/funded CPU rental and the renter wants to increase the expiration time
    if( words[1] == "extend_rental" ){
        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( words.size() >= 4, "memo for extend_rental operation is incomplete" );

        state3 s = state_s_3.get();

        int64_t profit_made = quantity.amount;

        //memo should also include account to rent to 
        const eosio::name cpu_receiver = eosio::name( words[2] );

        //make sure a record for this combo already exists
        auto renter_receiver_idx = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo = mix64to128(from.value, cpu_receiver.value);

        auto itr = renter_receiver_idx.require_find(renter_receiver_combo, "could not locate an existing rental for this renter/receiver combo");  

        //validate the length of the extension
        const uint64_t days_to_rent = std::strtoull( words[3].c_str(), NULL, 0 );
        check( days_to_rent >= 1, "extension must be at least 1 day" );
        check( days_to_rent <= MAXIMUM_CPU_RENTAL_DAYS, ( "maximum days to rent is " + std::to_string( MAXIMUM_CPU_RENTAL_DAYS ) ).c_str() );

        const int64_t wax_amount_to_rent = itr->amount_staked.amount;

        //make sure the rental is currently in progress
        check( itr->expires != 0, "you can't extend a rental if it hasnt been funded yet" );
        check( itr->expires > now(), "you can't extend a rental after it expired" );

        //calculate the cost of increasing the rental, and validate the payment
        int64_t amount_expected = cpu_rental_price( days_to_rent, s.cost_to_rent_1_wax.amount, wax_amount_to_rent );
        check( quantity.amount >= amount_expected, ("expected to receive " + asset( amount_expected, WAX_SYMBOL ).to_string() ).c_str() );

        //if they sent more than expected, calculate difference and refund it
        if( quantity.amount > amount_expected ){
            int64_t amount_to_refund = safeSubInt64( quantity.amount, amount_expected );

            if( amount_to_refund > 0 ){
                profit_made = safeSubInt64( quantity.amount, amount_to_refund);
                transfer_tokens( from, asset( amount_to_refund, WAX_SYMBOL ), WAX_CONTRACT, "cpu rental refund from waxfusion.io - liquid staking protocol" );
            }
        }        

        //update the renter row (expires)
        renter_receiver_idx.modify(itr, same_payer, [&](auto &_r){
            _r.expires += days_to_seconds( days_to_rent );
        });

        check( profit_made > 0, "error with rental cost calculation" );

        //send the profit to the dapp contract for distribution later
        transfer_tokens( DAPP_CONTRACT, asset( profit_made, WAX_SYMBOL ), WAX_CONTRACT, "waxfusion_revenue" );        

        //update the state
        state_s_3.set(s, _self);

        //Make sure we are voting at full strength for relevant BPs
        update_votes();
        return;
    } 

    //there is an existing CPU rental and the renter wants to increase the amount of WAX staked to the receiver
    if( words[1] == "increase_rental" ){
        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( words.size() >= 4, "memo for increase_rental operation is incomplete" );

        state3 s = state_s_3.get();
        
        int64_t profit_made = quantity.amount;

        //memo should also include account to rent to 
        const eosio::name cpu_receiver = eosio::name( words[2] );

        //make sure a record for this combo already exists
        auto renter_receiver_idx = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo = mix64to128(from.value, cpu_receiver.value);

        auto itr = renter_receiver_idx.require_find(renter_receiver_combo, "could not locate an existing rental for this renter/receiver combo");  

        int64_t existing_rental_amount = itr->amount_staked.amount;

        /** wax_amount_to_rent
         *  only "whole numbers" of wax can be rented
         *  therefore the memo accepts an integer that isn't yet scaled
         *  i.e. '10' in the memo == 10.00000000 WAX
         *  we need to scale by 1e8 to get the correct number to create an asset
         */
        const uint64_t whole_number_of_wax_to_rent = std::strtoull( words[3].c_str(), NULL, 0 );
        check( whole_number_of_wax_to_rent >= safeDivUInt64( MINIMUM_WAX_TO_INCREASE, uint64_t(SCALE_FACTOR_1E8) ), ("minimum wax amount to increase is " + asset( int64_t(MINIMUM_WAX_TO_INCREASE), WAX_SYMBOL ).to_string() ).c_str() );

        const uint64_t wax_amount_to_rent = safeMulUInt64( whole_number_of_wax_to_rent, uint64_t(SCALE_FACTOR_1E8) );

        //combine the new request with the existing amount and validate that it's within the acceptable range
        const int64_t combined_wax_amount = safeAddInt64( int64_t(wax_amount_to_rent), int64_t(existing_rental_amount) );
        check( combined_wax_amount <= int64_t(MAXIMUM_WAX_TO_RENT), ( "maximum wax amount to rent is " + asset( int64_t(MAXIMUM_WAX_TO_RENT), WAX_SYMBOL ).to_string() ).c_str() );

        //make sure that this rental has already been funded
        check( itr->expires != 0, "you can't increase a rental if it hasnt been funded yet" );

        //make sure the rental is not expired, and find out how long is left
        check( itr->expires > now(), "this rental has already expired" );
        uint64_t seconds_remaining = itr->expires - now();

        //if there is less than a day left, reject the request
        check( seconds_remaining >= SECONDS_PER_DAY, "cant increase rental with < 1 day remaining" );

        //make sure we have enough wax available for this stake
        check( s.wax_available_for_rentals.amount >= int64_t(wax_amount_to_rent), "there is not enough wax in the rental pool to cover this rental" );

        //debit the wax from the rental pool and update the amount in use
        s.wax_available_for_rentals.amount = safeSubInt64( s.wax_available_for_rentals.amount, int64_t(wax_amount_to_rent) );
        s.wax_allocated_to_rentals.amount = safeAddInt64( s.wax_allocated_to_rentals.amount, int64_t(wax_amount_to_rent) );

        //calculate the cost of the rental and validate the payment amount
        int64_t amount_expected = cpu_rental_price_from_seconds( seconds_remaining, s.cost_to_rent_1_wax.amount, wax_amount_to_rent );
        check( quantity.amount >= amount_expected, ("expected to receive " + asset( amount_expected, WAX_SYMBOL ).to_string() ).c_str() );

        //if they sent more than expected, calculate difference and refund it
        if( quantity.amount > amount_expected ){
            int64_t amount_to_refund = safeSubInt64( quantity.amount, amount_expected );

            if( amount_to_refund > 0 ){
                profit_made = safeSubInt64( quantity.amount, amount_to_refund );
                transfer_tokens( from, asset( amount_to_refund, WAX_SYMBOL ), WAX_CONTRACT, "cpu rental refund from waxfusion.io - liquid staking protocol" );
            }
        }

        //stake cpu to the receiver
        stake_wax( cpu_receiver, int64_t(wax_amount_to_rent), 0 );

        //update the renter row (amount staked and expires)
        renter_receiver_idx.modify(itr, same_payer, [&](auto &_r){
            _r.amount_staked.amount = safeAddInt64( _r.amount_staked.amount, int64_t(wax_amount_to_rent) );
        });

        check( profit_made > 0, "error with rental cost calculation" );

        //Send the profits to the dapp contract so it can be used for distribution later
        transfer_tokens( DAPP_CONTRACT, asset( profit_made, WAX_SYMBOL ), WAX_CONTRACT, "waxfusion_revenue" );                

        //update the state
        state_s_3.set(s, _self);

        //Make sure we are voting at full strength for relevant BPs
        update_votes();
        return;
    }



    //below is handling for any cases where the memo has not been caught yet
    //need to be 100% sure that each case above has a return statement or else this will cause issues
    //anything below here should be fair game to add to the wax bucket

    state3 s = state_s_3.get();
    s.wax_bucket.amount = safeAddInt64( s.wax_bucket.amount, quantity.amount );
    state_s_3.set(s, _self);    
}

void polcontract::receive_lswax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
    if( quantity.amount == 0 ) return;

    const name tkcontract = get_first_receiver();

    check( quantity.amount > 0, "Must send a positive quantity" );
    check( quantity.amount < MAX_ASSET_AMOUNT, "quantity too large" );

    if( from == get_self() || to != get_self() ){
        return;
    }

    check( quantity.symbol == LSWAX_SYMBOL, "was expecting LSWAX token" );

    update_state();

    if( memo == "liquidity" && ( from == DAPP_CONTRACT || DEBUG ) ){

        state3 s = state_s_3.get();
        config2 c = config_s_2.get();
        dapp_tables::state ds = dapp_state_s.get();

        //add the lswax into the bucket
        s.lswax_bucket.amount = safeAddInt64( s.lswax_bucket.amount, quantity.amount );

        //get details about lswax price on alcor and the dapp contract
        liquidity_struct lp_details = get_liquidity_info( c, ds );

        //if alcor's price is in range and we have wax to pair with this lswax
        if( lp_details.is_in_range && s.wax_bucket > ZERO_WAX ){
            //state is passed as reference so it can be updated in the add_liquidity function
            add_liquidity( s, lp_details );
            s.last_liquidity_addition_time = now();
        }

        //if alcor's price is out of range or we dont have wax to pair with this lswax,
        //the block above wont get executed and the lswax will just stay in the bucket

        state_s_3.set(s, _self);
        return;
    }

    /** 
    * all LSWAX we receive should be fair game to add to the bucket, unless we come up 
    * with other reasons later
    */

    state3 s = state_s_3.get();
    s.lswax_bucket.amount = safeAddInt64( s.lswax_bucket.amount, quantity.amount );
    state_s_3.set(s, _self);

}