#pragma once

void mocksystem::receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){

	if( from == _self || to != _self ) return;

	check( quantity.amount > 0, "send a positive quantity" );

	//after a user delegates bw,
	//our unit tests should send the wax with this memo
	//to mock moving wax back and forth
	//then when we call the refund action,
	//this contract can send funds back with 
	//the "unstake" memo
	if( memo == "stake" ){
		auto itr = voters_t.require_find( from.value, "you have not staked anything yet" );

		voters_t.modify( itr, same_payer, [&](auto &_row){
			_row.amount_deposited += quantity;
		});

		return;
	}


}