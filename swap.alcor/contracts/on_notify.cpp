#pragma once

void alcor::receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
	//if pol contract sends us funds, we will add them to the bucket
	const name tkcontract = get_first_receiver();

	if( memo == "deposit" ){

		auto itr = pools_t.require_find(2, "alcor cannot locate pool 2");
		uint128_t sqrtPriceX64 = itr->currSlot.sqrtPriceX64;

		if( tkcontract == WAX_CONTRACT ){

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
		} else if( tkcontract == TOKEN_CONTRACT ){

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

	else if( memo.starts_with("incentreward#") ){
		const uint64_t 	incentive_id 	= get_incentive_id_from_string(memo);
		auto  			incentive_itr 	= incentives_t.require_find(incentive_id, "incentive not found");

		if(incentive_itr->periodFinish > now()){
			check(incentive_itr->reward.quantity.amount == 0, "rewards are still in the farm, cant deposit twice");
		}
		
		check(tkcontract == incentive_itr->reward.contract, "contract doesnt match incentive");
		check(quantity.symbol == incentive_itr->reward.quantity.symbol, "symbol doesnt match incentive");

		incentives_t.modify(incentive_itr, same_payer, [&](auto &_i){
			_i.reward.quantity 	= quantity;
			_i.periodFinish 	= now() + incentive_itr->rewardsDuration;
		});
		return;
	}


}