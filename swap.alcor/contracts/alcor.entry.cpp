#pragma once

#include "alcor.hpp"
#include "on_notify.cpp"

//contractName: alcor

uint128_t alcor::calculate_sqrtPriceX64(int64_t amountA, int64_t amountB){
		if(amountA == amountB) return SQRT_64_1_TO_1;
		else if( (amountA == 0 && amountB != 0) || (amountB == 0 && amountA != 0) ) return 10;

    double priceRatio = static_cast<double>(amountA) / static_cast<double>(amountB);
    double sqrtPrice = sqrt(priceRatio);

    double scaled_sqrt = sqrtPrice * double(SCALE_FACTOR_1E8);
    uint64_t sqrt_int = uint64_t(scaled_sqrt);

    uint128_t sqrtPriceX64 = ( uint128_t(sqrt_int) * TWO_POW_64 ) / SCALE_FACTOR_1E8;

    return sqrtPriceX64;
}

uint64_t now(){
  return current_time_point().sec_since_epoch();
}

ACTION alcor::createpool(const eosio::name& account, const eosio::extended_asset& tokenA, const eosio::extended_asset& tokenB)
{
	require_auth( account );
	
	pools_t.emplace(_self, [&](auto &_row){
		_row.id = pools_t.available_primary_key();
		_row.active = true;
		_row.tokenA = tokenA;
		_row.tokenB = tokenB;
		_row.fee = 3000;
		_row.feeProtocol = 0;
		_row.tickSpacing = 60;
		_row.maxLiquidityPerTick = 1247497401346422;
		_row.currSlot = {SQRT_64_1_TO_1,1711149497,1,1};
		_row.feeGrowthGlobalAX64 = 0;
		_row.feeGrowthGlobalBX64 = 0;
		_row.protocolFeeA = asset(0, tokenA.quantity.symbol);
		_row.protocolFeeB = asset(0, tokenB.quantity.symbol);
		_row.liquidity = 0;
	});
}


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

	uint128_t sqrtPriceX64 = calculate_sqrtPriceX64(wax_amount.amount, lswax_amount.amount);	

	pools_t.emplace(_self, [&](auto &_row){
		_row.id = 2;
		_row.active = true;
		_row.tokenA = { wax_amount, WAX_CONTRACT };
		_row.tokenB = { lswax_amount, TOKEN_CONTRACT };
		_row.fee = 3000;
		_row.feeProtocol = 0;
		_row.tickSpacing = 60;
		_row.maxLiquidityPerTick = 1247497401346422;
		_row.currSlot = {sqrtPriceX64,1711149497,1,1};
		_row.feeGrowthGlobalAX64 = 0;
		_row.feeGrowthGlobalBX64 = 0;
		_row.protocolFeeA = asset(0, WAX_SYMBOL);
		_row.protocolFeeB = asset(0, LSWAX_SYMBOL);
		_row.liquidity = 0;
	});
}


ACTION alcor::newincentive(const name& creator, const uint64_t& poolId, const extended_asset& rewardToken, const uint32_t& duration){
	require_auth( creator );

	incentives_t.emplace( _self, [&](auto &_row){
		_row.id = incentives_t.available_primary_key();
		_row.creator = creator;
		_row.poolId = poolId;
		_row.reward = rewardToken;
		_row.periodFinish = now() + uint64_t(duration);
		_row.rewardsDuration = uint64_t(duration);
	});

}