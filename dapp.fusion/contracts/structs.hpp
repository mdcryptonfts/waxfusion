#pragma once

struct staker_struct {
	eosio::name  	wallet;
	eosio::asset  	swax_balance;
	eosio::asset  	claimable_wax;
	uint64_t  		last_update;
};