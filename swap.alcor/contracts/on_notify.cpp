#pragma once

void alcor::receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
	//if pol contract sends us funds, we will add them to the bucket

	if( memo == "deposit" ){

		auto itr = pools_t.require_find(2, "alcor cannot locate pool 2");
		uint128_t sqrtPriceX64 = itr->currSlot.sqrtPriceX64;

		if( get_first_receiver() == WAX_CONTRACT ){

			int64_t new_qty_A = quantity.amount + itr->tokenA.quantity.amount;

			if(itr->tokenB.quantity.amount != 0){
				sqrtPriceX64 = calculate_sqrtPriceX64(new_qty_A, itr->tokenB.quantity.amount);
			}

			//add wax to the wax bucket
			pools_t.modify(itr, same_payer, [&](auto &_row){
				_row.tokenA.quantity += quantity;
				_row.currSlot.sqrtPriceX64 = sqrtPriceX64;
			});
			return;
		} else if( get_first_receiver() == TOKEN_CONTRACT ){

			int64_t new_qty_B = quantity.amount + itr->tokenB.quantity.amount;

			if(itr->tokenA.quantity.amount != 0){
				sqrtPriceX64 = calculate_sqrtPriceX64(itr->tokenA.quantity.amount, new_qty_B);
			}			

			//add lswax to the lswax bucket
			pools_t.modify(itr, same_payer, [&](auto &_row){
				_row.tokenB.quantity += quantity;
				_row.currSlot.sqrtPriceX64 = sqrtPriceX64;
			});		
			return;	
		}

	}


}