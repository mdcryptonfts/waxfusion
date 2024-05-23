#pragma once

void cpucontract::receive_system_request(const name& payer, const asset& wax_amount){
    if(!DEBUG) return;

    if(payer == _self){
        transfer_tokens( "eosio"_n, wax_amount, WAX_CONTRACT, "stake" );
    }
}

void cpucontract::receive_wax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
	const name tkcontract = get_first_receiver();

    check( quantity.amount > 0, "Must redeem a positive quantity" );
    check( quantity.amount < MAX_ASSET_AMOUNT, "quantity too large" );

    if( from == get_self() || to != get_self() ){
    	return;
    }

    check( quantity.symbol == WAX_SYMBOL, "was expecting WAX token" );

    //redundant check which isnt necessary when not using catchall notification handler
    check( tkcontract == WAX_CONTRACT, "first receiver should be eosio.token" );

    if( memo == "voter pay" ){

    	check( from == "eosio.voters"_n, "voter pay must come from eosio.voters" );
    	transfer_tokens( DAPP_CONTRACT, quantity, WAX_CONTRACT, std::string("waxfusion_revenue") );
        
        update_votes();
    	return;
    }

    if( memo == "unstake" ){

    	check( from == "eosio.stake"_n, "unstakes should come from eosio.stake" );
    	transfer_tokens( DAPP_CONTRACT, quantity, WAX_CONTRACT, std::string("cpu rental return") );

    	return;    	
    }

    std::vector<std::string> words = get_words(memo);

    if( words[1] == "stake_cpu"){

    	check( from == DAPP_CONTRACT, ("staking requests must come from " + DAPP_CONTRACT.to_string() ).c_str() );

    	const eosio::name cpu_receiver = eosio::name( words[2] );
    	const uint64_t epoch_timestamp = std::strtoull( words[3].c_str(), NULL, 0 );
        
    	action(permission_level{get_self(), "active"_n}, "eosio"_n,"delegatebw"_n,std::tuple{ get_self(), cpu_receiver, asset(0, WAX_SYMBOL), quantity, false}).send();

        update_votes();

    	return;
    }

}