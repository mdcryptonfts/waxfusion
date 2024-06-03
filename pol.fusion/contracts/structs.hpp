#pragma once

/** token_a_or_b
 *  stores info about liquidity on alcor
 *  and calculations related to adding liquidity
 *  during the add_liquidity and get_liquidity_info operations
 */

struct token_a_or_b {
	int64_t 		quantity;
	eosio::symbol  	symbol;
	eosio::name  	contract;
	eosio::asset  	minAsset;
	eosio::asset  	amountToAdd;
};

/** liquidity_struct
 *  stores token_a_or_b structs
 *  as well as other necessary info
 *  when creating positions on alcor
 */

struct liquidity_struct {
	bool  			is_in_range;
	int64_t  		alcors_lswax_price;
	int64_t  		real_lswax_price;
	bool  			aIsWax;
	token_a_or_b 	poolA;
	token_a_or_b    poolB;
	uint64_t  		poolId;
};