#pragma once

#include <constants.hpp>

struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] global {
    eosio::asset                swax_currently_earning;
    eosio::asset                swax_currently_backing_lswax;
    eosio::asset                liquified_swax;
    eosio::asset                revenue_awaiting_distribution;
    eosio::asset                total_revenue_distributed;
    eosio::asset                wax_for_redemption;
    uint64_t                    last_epoch_start_time;
    eosio::asset                wax_available_for_rentals;
    eosio::asset                cost_to_rent_1_wax;
    eosio::name                 current_cpu_contract;
    uint64_t                    next_stakeall_time;
    uint64_t                    last_incentive_distribution;
    eosio::asset                incentives_bucket;
    eosio::asset                total_value_locked;
    uint64_t                    total_shares_allocated;
    uint64_t                    last_compound_time;
    eosio::asset                minimum_stake_amount;
    eosio::asset                minimum_unliquify_amount;
    uint64_t                    initial_epoch_start_time;
    uint64_t                    cpu_rental_epoch_length_seconds;
    uint64_t                    seconds_between_epochs;
    uint64_t                    user_share_1e6;
    uint64_t                    pol_share_1e6;
    uint64_t                    ecosystem_share_1e6;
    std::vector<eosio::name>    admin_wallets;
    std::vector<eosio::name>    cpu_contracts;
    uint64_t                    redemption_period_length_seconds;
    uint64_t                    seconds_between_stakeall;
    eosio::name                 fallback_cpu_receiver;
    uint64_t                    protocol_fee_1e6;
    eosio::asset                total_rewards_claimed;


    EOSLIB_SERIALIZE(global,    (swax_currently_earning)
                                (swax_currently_backing_lswax)
                                (liquified_swax)
                                (revenue_awaiting_distribution)
                                (total_revenue_distributed)
                                (wax_for_redemption)
                                (last_epoch_start_time)
                                (wax_available_for_rentals)
                                (cost_to_rent_1_wax)
                                (current_cpu_contract)
                                (next_stakeall_time)
                                (last_incentive_distribution)
                                (incentives_bucket)
                                (total_value_locked)
                                (total_shares_allocated)
                                (last_compound_time)
                                (minimum_stake_amount)
                                (minimum_unliquify_amount)
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
                                (protocol_fee_1e6)
                                (total_rewards_claimed)
                    )
};
using global_singleton = eosio::singleton<"global"_n, global>;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] global2 {
    uint64_t        max_staker_apr_1e6      = 12000000;
    eosio::asset    minimum_new_incentive   = eosio::asset( 5000000000, LSWAX_SYMBOL );
    bool            panic                   = false;   // Not currently used, only here in case needed later
    bool            stake_unused_funds      = false;

    EOSLIB_SERIALIZE(global2,   (max_staker_apr_1e6)
                                (minimum_new_incentive)
                                (panic)
                                (stake_unused_funds)
                    )
};
using global_singleton_2 = eosio::singleton<"global2"_n, global2>;