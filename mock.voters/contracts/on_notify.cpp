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

void mockvoters::receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo){

	if( from == _self || to != _self ) return;

	check( quantity.amount > 0, "send a positive quantity" );

	std::vector<std::string> memo_parts = parse_memo( memo );

	//after a user delegates bw,
	//voting rewards should be sent here first (eosio.voters)
	//and then sent to the staker, since rewards should
	//come from eosio.voters and not eosio
	if( memo_parts[1] == "voter pay" ){
		check( memo_parts.size() >= 3, "memo for voter pay operation is incomplete" );

		check( from == "eosio"_n, "only eosio can send with this memo" );

		const eosio::name receiver = eosio::name( memo_parts[2] );

		transfer_tokens( receiver, quantity, WAX_CONTRACT, std::string("voter pay") );

		return;
	}


}