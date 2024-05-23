#pragma once
#define CONTRACT_NAME "cpucontract"
#define DEBUG true

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
#include "tables.hpp"
#include "constants.hpp"
#include <limits>

using namespace eosio;


CONTRACT cpucontract : public contract {
	public:
		using contract::contract;
		cpucontract(name receiver, name code, datastream<const char *> ds):
		contract(receiver, code, ds),
		state_s(receiver, receiver.value),
		top21_s(DAPP_CONTRACT, DAPP_CONTRACT.value)
		{}

		//Main Actions
		ACTION claimgbmvote();
		ACTION claimrefund();
		ACTION initstate();
		ACTION unstakebatch(const int& limit);

		//Notifications
		[[eosio::on_notify("eosio.token::transfer")]] void receive_wax_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);
		[[eosio::on_notify("eosio::requestwax")]] void receive_system_request(const name& payer, const asset& wax_amount);
		
	private:

		//Singletons
		state_singleton state_s;
		top21_singleton top21_s;


		//Functions
		std::vector<std::string> get_words(std::string memo);
		uint64_t now();
		void transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo);
		void update_votes();

};



