#pragma once

std::vector<std::string> parse_memo(std::string memo){
  std::string delim = "|";
  std::vector<std::string> words{};
  size_t pos = 0;
  while ((pos = memo.find(delim)) != std::string::npos) {
    words.push_back(memo.substr(0, pos));
    memo.erase(0, pos + delim.length());
  }
  return words;
}

void mockstake::receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){

	if( from == _self || to != _self ) return;

	check( quantity.amount > 0, "send a positive quantity" );

	std::vector<std::string> memo_parts = parse_memo( memo );

	//after a user initiates a refund claim,
	//eosio will send us the funds first, then
	//this contract can send funds back with 
	//the "unstake" memo since unstakes should come from eosio.stake
	if( memo_parts[1] == "unstake" ){
		check( memo_parts.size() >= 3, "memo for unstake operation is incomplete" );

		check( from == "eosio"_n, "only eosio can send with this memo" );

		const eosio::name receiver = eosio::name( memo_parts[2] );

		transfer_tokens( receiver, quantity, WAX_CONTRACT, std::string("unstake") );

		return;
	}


}