#pragma once

void fusion::handle_available_request(global& g, bool& request_can_be_filled, staker_struct& staker, asset& remaining_amount_to_fill){
    
    /** if there is currently a redemption window open, we need to check if
     *  this user has a request from this window. if they do, it should be
     *  automatically claimed and transferred to them, to avoid tying up
     *  2x the redemption amount and causing issues for other users who
     *  need to redeem
     */

    requests_tbl    requests_t              = requests_tbl(get_self(), staker.wallet.value);
    uint64_t        redemption_start_time   = g.last_epoch_start_time;
    uint64_t        redemption_end_time     = g.last_epoch_start_time + g.redemption_period_length_seconds;
    uint64_t        epoch_to_claim_from     = g.last_epoch_start_time - g.seconds_between_epochs;

    if ( now() < redemption_end_time ) {
        //there is currently a redemption window open
        //if this user has a request in that window, handle it before proceeding

        auto epoch_itr  = epochs_t.find( epoch_to_claim_from );
        auto req_itr    = requests_t.find( epoch_to_claim_from );

        if ( epoch_itr != epochs_t.end() && req_itr != requests_t.end() ) {

                check( req_itr->wax_amount_requested.amount <= staker.swax_balance.amount, "you have a pending request > your swax balance" );

                //make sure the redemption pool >= the request amount
                //the only time this should ever happen is if the CPU contract has not returned the funds yet, which should never really
                //be more than a 5-10 minute window on a given week
                check( g.wax_for_redemption.amount >= req_itr->wax_amount_requested.amount, "redemption pool is < your pending request" );

                if ( req_itr->wax_amount_requested.amount >= remaining_amount_to_fill.amount ) {
                    request_can_be_filled = true;
                } else {
                    remaining_amount_to_fill.amount -= req_itr->wax_amount_requested.amount;
                }

                g.wax_for_redemption -= req_itr->wax_amount_requested;

                epochs_t.modify( epoch_itr, same_payer, [&](auto & _e) {
                    _e.wax_to_refund -= req_itr->wax_amount_requested;
                });

                transfer_tokens( staker.wallet, req_itr->wax_amount_requested, WAX_CONTRACT, std::string("your redemption from waxfusion.io - liquid staking protocol") );

                staker.swax_balance.amount -= req_itr->wax_amount_requested.amount;

                requests_t.erase( req_itr );
        }
    }   
}

void fusion::handle_new_request(vector<uint64_t>& epochs_to_check, bool& request_can_be_filled, staker_struct& staker, asset& remaining_amount_to_fill){
    
    requests_tbl requests_t = requests_tbl(get_self(), staker.wallet.value);

    for (uint64_t ep : epochs_to_check) {
        auto epoch_itr = epochs_t.find(ep);

        if (epoch_itr != epochs_t.end()) {

            if (epoch_itr->redemption_period_start_time > now() &&  epoch_itr->wax_to_refund < epoch_itr->wax_bucket ) {

                //there are still funds available to redeem from this epoch

                int64_t amount_available = safecast::sub(epoch_itr->wax_bucket.amount, epoch_itr->wax_to_refund.amount);

                if (amount_available >= remaining_amount_to_fill.amount) {

                    request_can_be_filled = true;

                    int64_t updated_refunding_amount = safecast::add(epoch_itr->wax_to_refund.amount, remaining_amount_to_fill.amount);

                    epochs_t.modify(epoch_itr, get_self(), [&](auto & _e) {
                        _e.wax_to_refund = asset(updated_refunding_amount, WAX_SYMBOL);
                    });

                    requests_t.emplace(staker.wallet, [&](auto & _r) {
                        _r.epoch_id = ep;
                        _r.wax_amount_requested = asset(remaining_amount_to_fill.amount, WAX_SYMBOL);
                    });

                } else {
                    //this epoch has some funds, but not enough for the whole request
                    int64_t updated_refunding_amount = safecast::add(epoch_itr->wax_to_refund.amount, amount_available);

                    remaining_amount_to_fill.amount -= amount_available;

                    epochs_t.modify(epoch_itr, get_self(), [&](auto & _e) {
                        _e.wax_to_refund = asset(updated_refunding_amount, WAX_SYMBOL);
                    });

                    requests_t.emplace(staker.wallet, [&](auto & _r) {
                        _r.epoch_id = ep;
                        _r.wax_amount_requested = asset(amount_available, WAX_SYMBOL);
                    });
                }
            }

        }

        if ( request_can_be_filled ) break;
    }   
}

void fusion::remove_existing_requests(vector<uint64_t>& epochs_to_check, staker_struct& staker, const bool& accept_replacing_prev_requests){

    requests_tbl requests_t = requests_tbl(get_self(), staker.wallet.value);

    for (uint64_t ep : epochs_to_check) {

        auto epoch_itr = epochs_t.find(ep);
        auto req_itr = requests_t.find(ep);

        if ( epoch_itr != epochs_t.end() && req_itr != requests_t.end() ) {

            check(accept_replacing_prev_requests, "you have previous requests but passed 'false' to the accept_replacing_prev_requests param");
            
            epochs_t.modify(epoch_itr, get_self(), [&](auto & _e) {
                _e.wax_to_refund -= req_itr->wax_amount_requested;
            });

            requests_t.erase(req_itr);
            
        }
    }   
}