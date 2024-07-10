#pragma once

void fusion::receive_token_transfer(name from, name to, eosio::asset quantity, std::string memo) {

    if ( quantity.amount == 0 || from == get_self() || to != get_self() ) return;

    check( quantity.amount > 0, "must send a positive quantity" );
    check( quantity.amount < MAX_ASSET_AMOUNT, "quantity too large" );

    const name tkcontract = get_first_receiver();
    if ( tkcontract != WAX_CONTRACT && tkcontract != TOKEN_CONTRACT ) return;
    check( is_lswax_or_wax( quantity.symbol, tkcontract ), "only WAX and lsWAX are accepted" );

    if ( !memo_is_expected( memo ) && from != "eosio"_n ) {
        //if we reached here, the token is either wax or lswax, but the memo is not expected
        //allow transfers from eosio with unexpected memos during testing       
        check( false, "must include a memo for transfers to dapp.fusion, see docs.waxfusion.io for a list of memos" );
    }

    /** instant redeem and rebalance memos
     *  there are 2 cases where the POL contract will need to send LSWAX with these memos
     *  - if alcor price is out of range and it's been 1 week since last liquidity addition,
     *      in which case the returned wax will go to the CPU rental pool
     *  - if alcor price is in range and POL needs to rebalance its assets before depositing
     *      liquidity, in which case the returned funds will be used for liquidity instead
     */

    if ( memo == "instant redeem" || memo == "rebalance" ) {

        check( tkcontract == TOKEN_CONTRACT, "only LSWAX should be sent with this memo" );
        check( from == POL_CONTRACT, ( "expected " + POL_CONTRACT.to_string() + " to be the sender" ).c_str() );

        global  g = global_s.get();
        rewards r = rewards_s.get();

        sync_epoch( g );

        check( quantity >= g.minimum_unliquify_amount, "minimum unliquify amount not met" );

        auto            self_staker_itr = staker_t.require_find( _self.value, ERR_STAKER_NOT_FOUND );
        staker_struct   self_staker     = staker_struct(*self_staker_itr);

        extend_reward(g, r, self_staker);
        update_reward(self_staker, r);      

        int64_t swax_to_redeem = calculate_swax_output( quantity.amount, g );
        check( g.wax_available_for_rentals.amount >= swax_to_redeem, "not enough instaredeem funds available" );

        self_staker.swax_balance -= asset(swax_to_redeem, SWAX_SYMBOL);
        modify_staker(self_staker);

        r.totalSupply -= uint128_t(swax_to_redeem);

        int64_t protocol_share  = calculate_asset_share( swax_to_redeem, g.protocol_fee_1e6 );
        int64_t user_share      = swax_to_redeem - protocol_share;
        check( safecast::add( protocol_share, user_share ) <= swax_to_redeem, "error calculating protocol fee" );

        g.wax_available_for_rentals.amount      -= swax_to_redeem;
        g.revenue_awaiting_distribution.amount  += protocol_share;
        g.swax_currently_backing_lswax.amount   -= swax_to_redeem;
        g.liquified_swax.amount                 -= quantity.amount;

        global_s.set(g, _self);
        rewards_s.set(r, _self);

        retire_swax(swax_to_redeem);
        retire_lswax(quantity.amount);

        std::string transfer_memo = memo == "instant redeem" ? "for staking pool only" : "rebalance";
        transfer_tokens( from, asset( user_share, WAX_SYMBOL ), WAX_CONTRACT, transfer_memo );

        return;
    }

    if ( memo == "wax_lswax_liquidity" ) {

        check( tkcontract == WAX_CONTRACT, "only WAX should be sent with this memo" );
        check( from == POL_CONTRACT, ( "expected " + POL_CONTRACT.to_string() + " to be the sender" ).c_str() );

        global  g = global_s.get();
        rewards r = rewards_s.get();

        sync_epoch( g );

        auto            self_staker_itr = staker_t.require_find( _self.value, ERR_STAKER_NOT_FOUND );
        staker_struct   self_staker     = staker_struct(*self_staker_itr);

        extend_reward(g, r, self_staker);
        update_reward(self_staker, r);          

        self_staker.swax_balance += asset(quantity.amount, SWAX_SYMBOL);
        modify_staker(self_staker); 

        r.totalSupply += uint128_t(quantity.amount);        

        int64_t converted_lsWAX_i64 = calculate_lswax_output(quantity.amount, g);

        g.swax_currently_backing_lswax.amount   += quantity.amount;
        g.liquified_swax.amount                 += converted_lsWAX_i64;
        g.wax_available_for_rentals             += quantity;

        global_s.set(g, _self);
        rewards_s.set(r, _self);

        issue_swax(quantity.amount);
        issue_lswax(converted_lsWAX_i64, _self);
        transfer_tokens( POL_CONTRACT, asset( converted_lsWAX_i64, LSWAX_SYMBOL ), TOKEN_CONTRACT, "liquidity" );

        return;
    }

    /** stake memo
     *  used for creating new sWAX tokens at 1:1 ratio
     *  these sWAX will be staked initially, (added to the wax_available_for_rentals balance)
     *  they can be converted to liquid sWAX (lsWAX) afterwards
     */

    if ( memo == "stake" ) {
        check( tkcontract == WAX_CONTRACT, "only WAX is used for staking" );

        global g = global_s.get();
        rewards r = rewards_s.get();

        sync_epoch( g );

        check( quantity >= g.minimum_stake_amount, "minimum stake amount not met" );

        auto [staker, self_staker] = get_stakers(from, _self);

        extend_reward(g, r, self_staker);
        update_reward(staker, r);
        update_reward(self_staker, r);

        r.totalSupply += uint128_t(quantity.amount);

        staker.swax_balance.amount += quantity.amount;
        modify_staker(staker);
        modify_staker(self_staker);

        g.swax_currently_earning.amount += quantity.amount;
        g.wax_available_for_rentals     += quantity;
        
        rewards_s.set(r, _self);
        global_s.set(g, _self);

        issue_swax(quantity.amount);

        return;
    }

    /** unliquify memo
     *  used for converting lsWAX back to sWAX
     *  rate is not 1:1, needs to be fetched from state table
     */

    if ( memo == "unliquify" ) {

        global  g = global_s.get();
        rewards r = rewards_s.get();

        sync_epoch( g );

        check( tkcontract == TOKEN_CONTRACT, "only LSWAX can be unliquified" );
        check( quantity >= g.minimum_unliquify_amount, "minimum unliquify amount not met" );

        int64_t converted_sWAX_i64 = calculate_swax_output(quantity.amount, g);

        auto [staker, self_staker] = get_stakers(from, _self);

        extend_reward(g, r, self_staker);
        update_reward(staker, r);
        update_reward(self_staker, r);

        staker.swax_balance += asset(converted_sWAX_i64, SWAX_SYMBOL);
        self_staker.swax_balance -= asset(converted_sWAX_i64, SWAX_SYMBOL); 

        modify_staker(staker);
        modify_staker(self_staker);     

        g.liquified_swax -= quantity;
        g.swax_currently_backing_lswax.amount -= converted_sWAX_i64;
        g.swax_currently_earning.amount += converted_sWAX_i64;
        
        rewards_s.set(r, _self);
        global_s.set(g, _self);

        retire_lswax(quantity.amount);

        return;

    }

    /** waxfusion_revenue
     *  used for receiving revenue from helper contracts, like CPU rentals, wax staking, etc
     */

    if ( memo == "waxfusion_revenue" ) {
        check( tkcontract == WAX_CONTRACT, "only WAX is accepted with waxfusion_revenue memo" );

        global g = global_s.get();
        g.revenue_awaiting_distribution += quantity;
        global_s.set(g, _self);

        return;
    }

    /** lp_incentives
     *  to be used as rewards for creating farms on alcor
     *  accepts wax deposits and lsWAX deposits
     */

    if ( memo == "lp_incentives" ) {

        global g = global_s.get();
        rewards r = rewards_s.get();

        sync_epoch( g );

        if ( tkcontract == WAX_CONTRACT ) {

            int64_t converted_lsWAX_i64 = calculate_lswax_output( quantity.amount, g );

            issue_swax(quantity.amount);
            issue_lswax(converted_lsWAX_i64, _self);

            g.incentives_bucket.amount += converted_lsWAX_i64;
            g.wax_available_for_rentals += quantity;
            g.swax_currently_backing_lswax.amount += quantity.amount;
            g.liquified_swax.amount += converted_lsWAX_i64;

            auto self_staker_itr = staker_t.require_find( _self.value, ERR_STAKER_NOT_FOUND );
            staker_struct self_staker = staker_struct(*self_staker_itr);

            extend_reward(g, r, self_staker);
            update_reward(self_staker, r);  

            self_staker.swax_balance += asset(quantity.amount, SWAX_SYMBOL);
            modify_staker(self_staker);

            r.totalSupply += uint128_t(quantity.amount);            

        } else if ( tkcontract == TOKEN_CONTRACT ) {
            g.incentives_bucket += quantity;
        }

        global_s.set(g, _self);
        rewards_s.set(r, _self);
        return;
    }

    /** cpu rental return
     *  these are funds coming back from one of the CPU rental contracts after being staked/rented
     */

    if ( memo == "cpu rental return" ) {

        global g = global_s.get();

        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( is_cpu_contract(g, from), "sender is not a valid cpu rental contract" );

        sync_epoch( g );

        /**
        * this SHOULD always belong to last epoch - 2 epochs
        * i.e. by the time this arrives from epoch 1,
        * it should now be epoch 3
        */

        uint64_t relevant_epoch = g.last_epoch_start_time - g.cpu_rental_epoch_length_seconds;

        auto epoch_itr = epochs_t.require_find(relevant_epoch, "could not locate relevant epoch");

        while ( epoch_itr->cpu_wallet != from && epoch_itr != epochs_t.begin() ) {
            epoch_itr --;
        }

        check( epoch_itr->cpu_wallet == from, "sender does not match wallet linked to epoch" );

        asset total_added_to_redemption_bucket = epoch_itr->total_added_to_redemption_bucket;
        asset amount_to_send_to_rental_bucket = quantity;

        if ( epoch_itr->total_added_to_redemption_bucket < epoch_itr->wax_to_refund ) {
            //there are still funds to add to the redemption pool so users can redeem
            int64_t amount_added = 0;

            int64_t amount_remaining_to_add = safecast::sub(epoch_itr->wax_to_refund.amount, epoch_itr->total_added_to_redemption_bucket.amount);

            if (amount_remaining_to_add >= quantity.amount) {
                amount_added = quantity.amount;
            } else {
                amount_added = amount_remaining_to_add;
            }

            total_added_to_redemption_bucket.amount = safecast::add(total_added_to_redemption_bucket.amount, amount_added);
            amount_to_send_to_rental_bucket.amount = safecast::sub(amount_to_send_to_rental_bucket.amount, amount_added);
            g.wax_for_redemption.amount = safecast::add(g.wax_for_redemption.amount, amount_added);
        }

        if (amount_to_send_to_rental_bucket.amount > 0) {
            g.wax_available_for_rentals += amount_to_send_to_rental_bucket;
        }

        asset total_cpu_funds_returned = epoch_itr->total_cpu_funds_returned;
        total_cpu_funds_returned += quantity;

        epochs_t.modify(epoch_itr, get_self(), [&](auto & _e) {
            _e.total_cpu_funds_returned = total_cpu_funds_returned;
            _e.total_added_to_redemption_bucket = total_added_to_redemption_bucket;
        });

        global_s.set(g, _self);
        return;
    }

    /**
    * @get_words
    * anything below here should be a dynamic memo with multiple words to parse
    */

    std::vector<std::string> words = get_words( memo );

    if ( words[1] == "rent_cpu" ) {
        check( tkcontract == WAX_CONTRACT, "only WAX can be sent with this memo" );
        check( words.size() >= 5, "memo for rent_cpu operation is incomplete" );

        const eosio::name cpu_receiver = eosio::name( words[2] );
        check( is_account( cpu_receiver ), ( cpu_receiver.to_string() + " is not an account" ).c_str() );

        const uint64_t wax_amount_to_rent = std::strtoull( words[3].c_str(), NULL, 0 );
        const uint64_t amount_to_rent_with_precision = safecast::mul( wax_amount_to_rent, uint64_t(SCALE_FACTOR_1E8) );
        check( wax_amount_to_rent >= MINIMUM_WAX_TO_RENT, ( "minimum wax amount to rent is " + std::to_string( MINIMUM_WAX_TO_RENT ) ).c_str() );
        check( wax_amount_to_rent <= MAXIMUM_WAX_TO_RENT, ( "maximum wax amount to rent is " + std::to_string( MAXIMUM_WAX_TO_RENT ) ).c_str() );       

        const uint64_t epoch_id_to_rent_from = std::strtoull( words[4].c_str(), NULL, 0 );
        auto epoch_itr = epochs_t.require_find( epoch_id_to_rent_from, ("epoch " + std::to_string(epoch_id_to_rent_from) + " does not exist").c_str() );

        global g = global_s.get();

        sync_epoch( g );

        check( g.wax_available_for_rentals.amount >= amount_to_rent_with_precision, "there is not enough wax in the rental pool to cover this rental" );
        g.wax_available_for_rentals.amount -= int64_t(amount_to_rent_with_precision);

        uint64_t seconds_to_rent = get_seconds_to_rent_cpu(g, epoch_id_to_rent_from);

        int64_t expected_amount_received = g.cost_to_rent_1_wax.amount * wax_amount_to_rent * seconds_to_rent / days_to_seconds(1);

        check( quantity.amount >= expected_amount_received, ( "expected to receive " + eosio::asset( expected_amount_received, WAX_SYMBOL ).to_string() ).c_str() );
        g.revenue_awaiting_distribution.amount += expected_amount_received;

        if ( quantity.amount > expected_amount_received ) {

            int64_t amount_to_refund = safecast::sub(quantity.amount, expected_amount_received);
            transfer_tokens( from, asset( amount_to_refund, WAX_SYMBOL ), WAX_CONTRACT, "cpu rental refund from waxfusion.io - liquid staking protocol" );
        
        }

        transfer_tokens( epoch_itr->cpu_wallet, asset( (int64_t) amount_to_rent_with_precision, WAX_SYMBOL), WAX_CONTRACT, cpu_stake_memo(cpu_receiver, epoch_id_to_rent_from) );

        epochs_t.modify(epoch_itr, get_self(), [&](auto & _e) {
            _e.wax_bucket.amount += (int64_t) amount_to_rent_with_precision;
        });

        renters_table renters_t = renters_table( _self, epoch_id_to_rent_from );
        auto renter_receiver_idx = renters_t.get_index<"fromtocombo"_n>();
        const uint128_t renter_receiver_combo = mix64to128(from.value, cpu_receiver.value);

        auto rental_itr = renter_receiver_idx.find(renter_receiver_combo);

        if ( rental_itr == renter_receiver_idx.end() ) {
            renters_t.emplace(_self, [&](auto & _r) {
              _r.ID = renters_t.available_primary_key();
              _r.renter = from;
              _r.rent_to_account = cpu_receiver;
              _r.amount_staked = eosio::asset( int64_t(amount_to_rent_with_precision), WAX_SYMBOL );
            });
        } else {
            renter_receiver_idx.modify(rental_itr, _self, [&](auto & _r) {
              _r.amount_staked.amount += int64_t(amount_to_rent_with_precision);
            });
        }

        global_s.set(g, _self);
        return;
    }

    if ( words[1] == "unliquify_exact" ) {

        check( tkcontract == TOKEN_CONTRACT, "only LSWAX can be unliquified" );
        check( words.size() >= 3, "memo for unliquify_exact operation is incomplete" );

        const uint64_t minimum_output = std::strtoull( words[2].c_str(), NULL, 0 );
        check( minimum_output > 0 && minimum_output <= MAX_ASSET_AMOUNT_U64, "minimum_output is out of range" );

        global g = global_s.get();
        rewards r = rewards_s.get();

        sync_epoch( g );

        check( quantity >= g.minimum_unliquify_amount, "minimum unliquify amount not met" );

        int64_t converted_sWAX_i64 = calculate_swax_output(quantity.amount, g);

        if( converted_sWAX_i64 < int64_t(minimum_output) ){
            check( false, "output would be " + asset(converted_sWAX_i64, SWAX_SYMBOL).to_string() + " but expected " + asset(int64_t(minimum_output), SWAX_SYMBOL).to_string() );
        }

        auto [staker, self_staker] = get_stakers(from, _self);

        extend_reward(g, r, self_staker);
        update_reward(staker, r);
        update_reward(self_staker, r);

        staker.swax_balance += asset(converted_sWAX_i64, SWAX_SYMBOL);
        self_staker.swax_balance -= asset(converted_sWAX_i64, SWAX_SYMBOL);

        modify_staker(staker);
        modify_staker(self_staker);

        g.liquified_swax -= quantity;
        g.swax_currently_backing_lswax.amount -= converted_sWAX_i64;
        g.swax_currently_earning.amount += converted_sWAX_i64;
        
        global_s.set(g, _self);
        rewards_s.set(r, _self);

        retire_lswax( quantity.amount );
        return;
    }

}