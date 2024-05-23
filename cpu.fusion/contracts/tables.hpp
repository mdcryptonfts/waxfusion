#pragma once

// Every user 'from' has a scope/table that uses every recipient 'to' as the primary key.
struct [[eosio::table]] delegated_bandwidth {
  eosio::name          from;
  eosio::name          to;
  eosio::asset         net_weight;
  eosio::asset         cpu_weight;

  bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0; }
  uint64_t  primary_key()const { return to.value; }

  // explicit serialization macro is not necessary, used here only to improve compilation time
  EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight) )

};

typedef eosio::multi_index< "delband"_n, delegated_bandwidth > del_bandwidth_table;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] state {
  uint64_t          last_vote_time;

  EOSLIB_SERIALIZE(state, (last_vote_time))
};
using state_singleton = eosio::singleton<"state"_n, state>;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] top21 {
  std::vector<eosio::name>    block_producers;
  uint64_t                    last_update;

  EOSLIB_SERIALIZE(top21, (block_producers)(last_update))
};
using top21_singleton = eosio::singleton<"top21"_n, top21>;