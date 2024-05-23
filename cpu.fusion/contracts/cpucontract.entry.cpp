#pragma once

#include "cpucontract.hpp"
#include "functions.cpp"
#include "on_notify.cpp"

//contractName: cpucontract

ACTION cpucontract::claimgbmvote()
{
	require_auth( DAPP_CONTRACT );
	action(permission_level{get_self(), "active"_n}, "eosio"_n,"claimgbmvote"_n,std::tuple{ get_self() }).send();
}

ACTION cpucontract::claimrefund()
{
	require_auth( DAPP_CONTRACT );
	action(permission_level{get_self(), "active"_n}, "eosio"_n,"refund"_n,std::tuple{ get_self() }).send();
}

ACTION cpucontract::initstate(){
	require_auth( _self );
	eosio::check(!state_s.exists(), "state already exists");

	state s{};
	s.last_vote_time = 0;
	state_s.set(s, _self);
}

ACTION cpucontract::unstakebatch(const int& limit)
{
	require_auth( DAPP_CONTRACT );
	check( limit > 0, "limit must be a positive number" );

	del_bandwidth_table del_tbl( "eosio"_n, get_self().value );

	if( del_tbl.begin() == del_tbl.end() ){
		check( false, "nothing to unstake" );
	}

	int count = 0;
	for( auto itr = del_tbl.begin(); itr != del_tbl.end(); itr++ ){
		if( count == limit ) break;
		action(permission_level{get_self(), "active"_n}, "eosio"_n,"undelegatebw"_n,std::tuple{ get_self(), itr->to, itr->net_weight, itr->cpu_weight}).send();
		count ++;
	}

}