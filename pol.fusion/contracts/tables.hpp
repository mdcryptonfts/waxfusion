#pragma once

struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] config2 {
  uint64_t      liquidity_allocation_1e6;
  uint64_t      rental_pool_allocation_1e6;
  uint64_t      lswax_wax_pool_id;

  EOSLIB_SERIALIZE(config2, (liquidity_allocation_1e6)
                          (rental_pool_allocation_1e6)
                          (lswax_wax_pool_id)
                          )
};
using config_singleton_2 = eosio::singleton<"config2"_n, config2>;

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

struct [[eosio::table]] refund_request {
  eosio::name             owner;
  eosio::time_point_sec   request_time;
  eosio::asset            net_amount;
  eosio::asset            cpu_amount;

  bool is_empty()const { return net_amount.amount == 0 && cpu_amount.amount == 0; }
  uint64_t  primary_key()const { return owner.value; }

  // explicit serialization macro is not necessary, used here only to improve compilation time
  EOSLIB_SERIALIZE( refund_request, (owner)(request_time)(net_amount)(cpu_amount) )
};
typedef eosio::multi_index< "refunds"_n, refund_request >      refunds_table;


/**
* total bytes for a row is 560, except for the initial row which was 896
*/

struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] renters {
  uint64_t      ID;
  eosio::name   renter; 
  eosio::name   rent_to_account;
  eosio::asset  amount_staked;
  uint64_t expires;
  
  uint64_t primary_key() const { return ID; }
  uint64_t second_key() const { return renter.value; }
  uint64_t third_key() const { return expires; }
  uint128_t by_from_to_combo() const { return mix64to128( renter.value, rent_to_account.value ); }
};
using renters_table = eosio::multi_index<"renters"_n, renters,
eosio::indexed_by<"renter"_n, eosio::const_mem_fun<renters, uint64_t, &renters::second_key>>,
eosio::indexed_by<"expires"_n, eosio::const_mem_fun<renters, uint64_t, &renters::third_key>>,
eosio::indexed_by<"fromtocombo"_n, eosio::const_mem_fun<renters, uint128_t, &renters::by_from_to_combo>>
>;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] state3 {
  eosio::asset      wax_available_for_rentals;
  uint64_t          next_day_end_time;
  eosio::asset      cost_to_rent_1_wax;
  uint64_t          last_vote_time;
  eosio::asset      wax_bucket;
  eosio::asset      lswax_bucket;
  uint64_t          last_liquidity_addition_time;  
  eosio::asset      wax_allocated_to_rentals;
  eosio::asset      pending_refunds;
  uint64_t          last_rebalance_time;

  EOSLIB_SERIALIZE(state3,  
                          (wax_available_for_rentals)
                          (next_day_end_time)
                          (cost_to_rent_1_wax)
                          (last_vote_time)
                          (wax_bucket)
                          (lswax_bucket)
                          (last_liquidity_addition_time)
                          (wax_allocated_to_rentals)
                          (pending_refunds)
                          (last_rebalance_time)
                          )
};
using state_singleton_3 = eosio::singleton<"state3"_n, state3>;


struct [[eosio::table]] top21 {
  std::vector<eosio::name>    block_producers;
  uint64_t                    last_update;

  EOSLIB_SERIALIZE(top21, (block_producers)(last_update))
};
using top21_singleton = eosio::singleton<"top21"_n, top21>;