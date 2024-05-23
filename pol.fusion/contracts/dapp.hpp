#pragma once

namespace dapp_tables {

  struct [[eosio::table]] config3 {
    eosio::asset                      minimum_stake_amount;
    eosio::asset                      minimum_unliquify_amount;
    uint64_t                          seconds_between_distributions;
    uint64_t                          max_snapshots_to_process;
    uint64_t                          initial_epoch_start_time;
    uint64_t                          cpu_rental_epoch_length_seconds;
    uint64_t                          seconds_between_epochs; /* epochs overlap, this is 1 week */
    uint64_t                          user_share_1e6;
    uint64_t                          pol_share_1e6;
    uint64_t                          ecosystem_share_1e6;
    std::vector<eosio::name>          admin_wallets;
    std::vector<eosio::name>          cpu_contracts;
    uint64_t                          redemption_period_length_seconds;
    uint64_t                          seconds_between_stakeall;
    eosio::name                       fallback_cpu_receiver;

    EOSLIB_SERIALIZE(config3, (minimum_stake_amount)
                              (minimum_unliquify_amount)
                              (seconds_between_distributions)
                              (max_snapshots_to_process)
                              (initial_epoch_start_time)
                              (cpu_rental_epoch_length_seconds)
                              (seconds_between_epochs)
                              (user_share_1e6)
                              (pol_share_1e6)
                              (ecosystem_share_1e6)
                              (admin_wallets)
                              (cpu_contracts)
                              (redemption_period_length_seconds)
                              (seconds_between_stakeall)
                              (fallback_cpu_receiver)
                              )
  };
  using config_singleton_3 = eosio::singleton<"config3"_n, config3>;
  
  struct [[eosio::table]] state {
    eosio::asset      swax_currently_earning;
    eosio::asset      swax_currently_backing_lswax;
    eosio::asset      liquified_swax;
    eosio::asset      revenue_awaiting_distribution;
    eosio::asset      user_funds_bucket;
    eosio::asset      total_revenue_distributed;
    uint64_t          next_distribution;
    eosio::asset      wax_for_redemption;
    uint64_t          redemption_period_start; 
    uint64_t          redemption_period_end;
    uint64_t          last_epoch_start_time;
    eosio::asset      wax_available_for_rentals;
    eosio::asset      cost_to_rent_1_wax;
    eosio::name       current_cpu_contract;
    uint64_t          next_stakeall_time;

    EOSLIB_SERIALIZE(state, (swax_currently_earning)
                            (swax_currently_backing_lswax)
                            (liquified_swax)
                            (revenue_awaiting_distribution)
                            (user_funds_bucket)
                            (total_revenue_distributed)
                            (next_distribution)
                            (wax_for_redemption)
                            (redemption_period_start)
                            (redemption_period_end)
                            (last_epoch_start_time)
                            (wax_available_for_rentals)
                            (cost_to_rent_1_wax)
                            (current_cpu_contract)
                            (next_stakeall_time)
                            )
  };
  using state_singleton = eosio::singleton<"state"_n, state>;
 
}