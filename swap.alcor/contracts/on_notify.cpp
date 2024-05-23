#pragma once

void alcor::receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){
	//if pol contract sends us funds, we will add them to the bucket

	if( memo == "deposit" ){

		auto itr = pools_t.require_find(2, "alcor cannot locate pool 2");

		if( get_first_receiver() == WAX_CONTRACT ){
			//add wax to the wax bucket
			pools_t.modify(itr, same_payer, [&](auto &_row){
				_row.tokenA.quantity += quantity;
			});
			return;
		} else if( get_first_receiver() == TOKEN_CONTRACT ){
			//add lswax to the lswax bucket
			pools_t.modify(itr, same_payer, [&](auto &_row){
				_row.tokenB.quantity += quantity;
			});		
			return;	
		}

	}


}