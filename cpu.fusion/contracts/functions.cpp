#pragma once

std::vector<std::string> cpucontract::get_words(std::string memo){
  std::string delim = "|";
  std::vector<std::string> words{};
  size_t pos = 0;
  while ((pos = memo.find(delim)) != std::string::npos) {
    words.push_back(memo.substr(0, pos));
    memo.erase(0, pos + delim.length());
  }
  return words;
}

uint64_t cpucontract::now(){
  return current_time_point().sec_since_epoch();
}

void cpucontract::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo){
	action(permission_level{get_self(), "active"_n}, contract,"transfer"_n,std::tuple{ get_self(), user, amount_to_send, memo}).send();
}

void cpucontract::update_votes(){
	state s = state_s.get();

	if(s.last_vote_time + (60 * 60 * 24) <= now()){
		top21 t = top21_s.get();

		const eosio::name no_proxy;
		action(permission_level{get_self(), "active"_n}, "eosio"_n,"voteproducer"_n,std::tuple{ get_self(), no_proxy, t.block_producers }).send();

		s.last_vote_time = now();
		state_s.set(s, _self);
	}
}