#pragma once

#define CONTRACT_NAME "fusion"
#define DEBUG true
#define mix64to128(a, b) (uint128_t(a) << 64 | uint128_t(b))

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/symbol.hpp>
#include <eosio/crypto.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/producer_schedule.hpp>
#include "constants.hpp"
#include "safecast.hpp"
#include "tables.hpp"
#include "structs.hpp"
#include "global.hpp"
#include "voting.hpp"

using namespace eosio;
using namespace std;

CONTRACT fusion : public contract {
	public:
		using contract::contract;

		fusion(name receiver, name code, datastream<const char *> ds):
		contract(receiver, code, ds),
		global_s(receiver, receiver.value),
		rewards_s(receiver, receiver.value),
		pol_state_s_3(POL_CONTRACT, POL_CONTRACT.value),
		top21_s(receiver, receiver.value)
		{}		

		//Main Actions
		ACTION addadmin(const eosio::name& admin_to_add);
		ACTION addcpucntrct(const eosio::name& contract_to_add);
		ACTION claimaslswax(const eosio::name& user, const eosio::asset& minimum_output);
		ACTION claimgbmvote(const eosio::name& cpu_contract);
		ACTION claimrefunds();
		ACTION claimrewards(const eosio::name& user);
		ACTION claimswax(const eosio::name& user);
		ACTION compound();
		ACTION clearexpired(const eosio::name& user);
		ACTION createfarms();
		ACTION init(const asset& initial_reward_pool);
		ACTION inittop21();
		ACTION instaredeem(const eosio::name& user, const eosio::asset& swax_to_redeem);
		ACTION liquify(const eosio::name& user, const eosio::asset& quantity);
		ACTION liquifyexact(const eosio::name& user, const eosio::asset& quantity, 
			const eosio::asset& minimum_output);
		ACTION reallocate();
		ACTION redeem(const eosio::name& user);
		ACTION removeadmin(const eosio::name& admin_to_remove);
		ACTION reqredeem(const eosio::name& user, const eosio::asset& swax_to_redeem, const bool& accept_replacing_prev_requests);
		ACTION rmvcpucntrct(const eosio::name& contract_to_remove);
		ACTION rmvincentive(const uint64_t& poolId);
		ACTION setfallback(const eosio::name& caller, const eosio::name& receiver);
		ACTION setincentive(const uint64_t& poolId, const eosio::symbol& symbol_to_incentivize, const eosio::name& contract_to_incentivize, const uint64_t& percent_share_1e6);
		ACTION setpolshare(const uint64_t& pol_share_1e6);
		ACTION setredeemfee(const uint64_t& protocol_fee_1e6);
		ACTION setrentprice(const eosio::name& caller, const eosio::asset& cost_to_rent_1_wax);
		ACTION stake(const eosio::name& user);
		ACTION stakeallcpu();
		ACTION sync(const eosio::name& caller);
		ACTION unstakecpu(const uint64_t& epoch_id, const int& limit);
		ACTION updatetop21();

		//Readonly Actions
		[[eosio::action, eosio::read_only]] uint64_t showexpcpu(const uint64_t& epoch_id);
		[[eosio::action, eosio::read_only]] bool showrefunds();
		[[eosio::action, eosio::read_only]] asset showreward(const name& user);		
		[[eosio::action, eosio::read_only]] vector<name> showvoterwds();

		//Notifications
		[[eosio::on_notify("*::transfer")]] void receive_token_transfer(name from, name to, eosio::asset quantity, std::string memo);

	private:

		//Singletons
		pol_contract::state_singleton_3 pol_state_s_3;
		global_singleton global_s;
		rewards_singleton rewards_s;
		top21_singleton top21_s;

		//Multi Index Tables
		alcor_contract::incentives_table incentives_t = alcor_contract::incentives_table(ALCOR_CONTRACT, ALCOR_CONTRACT.value);
		alcor_contract::pools_table pools_t = alcor_contract::pools_table(ALCOR_CONTRACT, ALCOR_CONTRACT.value);
		epochs_table epochs_t = epochs_table(get_self(), get_self().value);
		lpfarms_table lpfarms_t = lpfarms_table(get_self(), get_self().value);
		producers_table _producers = producers_table(SYSTEM_CONTRACT, SYSTEM_CONTRACT.value);
		staker_table staker_t = staker_table(get_self(), get_self().value);


		//Functions
		inline eosio::permission_level active_perm();
		int64_t calculate_asset_share(const int64_t& quantity, const uint64_t& percentage);
		int64_t calculate_lswax_output(const int64_t& quantity, global& g);
		int64_t calculate_swax_output(const int64_t& quantity, global& g);		
		void create_alcor_farm(const uint64_t& poolId, const eosio::symbol& token_symbol, const eosio::name& token_contract);
		void create_epoch(const global& g, const uint64_t& start_time, const name& cpu_wallet, const asset& wax_bucket);
		uint64_t days_to_seconds(const uint64_t& days);
		void debit_user_redemptions_if_necessary(const name& user, const asset& swax_balance);
		std::string cpu_stake_memo(const eosio::name& cpu_receiver, const uint64_t& epoch_timestamp);
		eosio::name get_next_cpu_contract(global& g);
		uint64_t get_seconds_to_rent_cpu(global& g, const uint64_t& epoch_id_to_rent_from);
		std::vector<std::string> get_words(std::string memo);
		bool is_an_admin(global& g, const eosio::name& user);
		bool is_cpu_contract(global& g, const eosio::name& contract);
		bool is_lswax_or_wax(const eosio::symbol& symbol, const eosio::name& contract);
		void issue_lswax(const int64_t& amount, const eosio::name& receiver);
		void issue_swax(const int64_t& amount);
		bool memo_is_expected(const std::string& memo);
		inline uint64_t now();
		inline void readonly_sync_epoch(global& g);
		void retire_lswax(const int64_t& amount);
		void retire_swax(const int64_t& amount);
		inline void sync_epoch(global& g);
		void transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo);
		void validate_allocations( const int64_t& quantity, const std::vector<int64_t> allocations );

		//Redemptions
		void handle_available_request(global& g, bool& request_can_be_filled, staker_struct& staker, asset& remaining_amount_to_fill);
		void handle_new_request(vector<uint64_t>& epochs_to_check, bool& request_can_be_filled, staker_struct& staker, asset& remaining_amount_to_fill);
		void remove_existing_requests(vector<uint64_t>& epochs_to_check, staker_struct& staker, const bool& accept_replacing_prev_requests);

		//Staking
        int64_t earned(staker_struct& staker, rewards& r);
        void extend_reward(global&g, rewards& r, staker_struct& self_staker);
        std::pair<staker_struct, staker_struct> get_stakers(const eosio::name& user, const eosio::name& self);
        void modify_staker(staker_struct& staker);
        void readonly_extend_reward(global&g, rewards& r, staker_struct& self_staker);
        uint128_t reward_per_token(rewards& r);
        void update_reward(staker_struct& staker, rewards& r);	
        void zero_distribution(global& g, rewards& r);	

		//Safemath
		int64_t mulDiv(uint64_t a, uint64_t b, uint128_t denominator);
		uint128_t mulDiv128(const uint128_t& a, const uint128_t& b, const uint128_t& denominator);
};
