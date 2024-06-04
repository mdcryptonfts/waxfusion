#pragma once
#define CONTRACT_NAME "polcontract"
#define TESTNET true
#define DEBUG true
#define mix64to128(a, b) (uint128_t(a) << 64 | uint128_t(b))


#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/symbol.hpp>
#include <eosio/action.hpp>
#include <string>
#include <eosio/crypto.hpp>
#include <eosio/transaction.hpp>
#include <eosio/singleton.hpp>
#include <cmath>
#include "safecast.hpp"
#include "tables.hpp"
#include "alcor.hpp"
#include "dapp.hpp"
#include "structs.hpp"
#include "constants.hpp"
#include <limits>

using namespace eosio;


CONTRACT polcontract : public contract {
	public:
		using contract::contract;
		polcontract(name receiver, name code, datastream<const char *> ds):
		contract(receiver, code, ds),
		config_s_2(receiver, receiver.value),
		dapp_config_s(DAPP_CONTRACT, DAPP_CONTRACT.value),
		dapp_state_s(DAPP_CONTRACT, DAPP_CONTRACT.value),
		state_s_3(receiver, receiver.value),
		top21_s(DAPP_CONTRACT, DAPP_CONTRACT.value)
		{}

		//Main Actions
		ACTION claimgbmvote();
		ACTION claimrefund();
		ACTION clearexpired(const int& limit);
		ACTION deleterental(const uint64_t& rental_id);
		ACTION initconfig();
		ACTION initstate3();
		ACTION rebalance();
		ACTION rentcpu(const eosio::name& renter, const eosio::name& cpu_receiver);
		ACTION setallocs(const uint64_t& liquidity_allocation_percent_1e6);
		ACTION setrentprice(const eosio::asset& cost_to_rent_1_wax);

		//Notifications
		[[eosio::on_notify("eosio.token::transfer")]] void receive_wax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);
		[[eosio::on_notify("token.fusion::transfer")]] void receive_lswax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);

		//DEBUG notification to mimic eosio contract altering balance when staking cpu
		[[eosio::on_notify("eosio::requestwax")]] void receive_system_request(const name& payer, const asset& wax_amount);

	private:

		//Singletons
		config_singleton_2 config_s_2;
		dapp_tables::config_singleton_3 dapp_config_s;
		dapp_tables::state_singleton dapp_state_s;
		state_singleton_3 state_s_3;
		top21_singleton top21_s;

		//Multi Index Tables
		alcor_contract::pools_table pools_t = alcor_contract::pools_table(ALCOR_CONTRACT, ALCOR_CONTRACT.value);
		refunds_table refunds_t = refunds_table(SYSTEM_CONTRACT, get_self().value);
		renters_table renters_t = renters_table(get_self(), get_self().value);

		//Functions
		void add_liquidity( state3& s, liquidity_struct& lp_details );
		int64_t calculate_asset_share(const int64_t& quantity, const uint64_t& percentage);
		void calculate_liquidity_allocations(const liquidity_struct& lp_details, 
			int64_t& liquidity_allocation, int64_t& wax_bucket_allocation, int64_t& buy_lswax_allocation);
		int64_t cpu_rental_price(const uint64_t& days, const int64_t& price_per_day, const int64_t& amount);
		int64_t cpu_rental_price_from_seconds(const uint64_t& seconds, const int64_t& price_per_day, const uint64_t& amount);
		uint64_t days_to_seconds(const uint64_t& days);
		void deposit_liquidity_to_alcor(const liquidity_struct& lp_details);
		liquidity_struct get_liquidity_info(config2 c, dapp_tables::state ds);
		int64_t internal_liquify(const int64_t& quantity, dapp_tables::state s);
		int64_t internal_unliquify(const int64_t& quantity, dapp_tables::state s);
		uint64_t now();
		std::vector<std::string> parse_memo(std::string memo);
		uint128_t seconds_to_days_1e6(const uint64_t& seconds);
		std::vector<int64_t> sqrt64_to_price(const uint128_t& sqrtPriceX64);
		void stake_wax(const name& receiver, const int64_t& cpu_amount, const int64_t& net_amount);
		int64_t token_price(const int64_t& amount_A, const int64_t& amount_B);
		void transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo);
		void update_state();
		void update_votes();
		void validate_allocations(const int64_t& quantity, const std::vector<int64_t> allocations);

		//Safemath
		int64_t mulDiv(uint64_t a, uint64_t b, uint128_t denominator);
		int64_t safeAddInt64(const int64_t& a, const int64_t& b);
		uint128_t safeAddUInt128(const uint128_t& a, const uint128_t& b);
		int64_t safeDivInt64(const int64_t& a, const int64_t& b);
		uint64_t safeDivUInt64(const uint64_t& a, const uint64_t& b);
		uint128_t safeDivUInt128(const uint128_t& a, const uint128_t& b);
		uint128_t safeMulUInt128(const uint128_t& a, const uint128_t& b);
		uint64_t safeMulUInt64(const uint64_t& a, const uint64_t& b);
		int64_t safeSubInt64(const int64_t& a, const int64_t& b);
		uint128_t max_scale_with_room(const uint128_t& num);
		uint128_t max_scale_without_room(const uint128_t& num);

};
