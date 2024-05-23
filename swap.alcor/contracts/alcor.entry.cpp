#pragma once

#include "alcor.hpp"
#include "on_notify.cpp"

//contractName: alcor


/** since we dont have alcor's contract
 *  and all we need for unit tests is the existence of the lswax/wax pool in the table
 *  we will just mock this using the table struct and emplacing the pool ourselves
 */

ACTION alcor::initunittest(const eosio::asset& wax_amount, const eosio::asset& lswax_amount)
{
	//allow wiping the table and re-initializing with new amounts in separate unit tests
	auto itr = pools_t.find( 2 );
	if( itr != pools_t.end() ){
		pools_t.erase( itr );
	}

	pools_t.emplace(_self, [&](auto &_row){
		_row.id = 2;
		_row.active = true;
		_row.tokenA = { asset(wax_amount), WAX_CONTRACT };
		_row.tokenB = { asset(lswax_amount), TOKEN_CONTRACT };
		_row.fee = 3000;
		_row.feeProtocol = 0;
		_row.tickSpacing = 60;
		_row.maxLiquidityPerTick = 1247497401346422;
		_row.currSlot = 	{0,1711149497,1,1};
		_row.feeGrowthGlobalAX64 = 0;
		_row.feeGrowthGlobalBX64 = 0;
		_row.protocolFeeA = asset(0, WAX_SYMBOL);
		_row.protocolFeeB = asset(0, LSWAX_SYMBOL);
		_row.liquidity = 0;
	});
}
