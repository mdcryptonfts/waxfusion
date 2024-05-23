#pragma once
#define CONTRACT_NAME "mockvoters"

#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/symbol.hpp>
#include <eosio/action.hpp>
#include <string>
#include <eosio/crypto.hpp>
#include <eosio/transaction.hpp>
#include <cmath>
#include "constants.hpp"


using namespace eosio;


CONTRACT mockvoters : public contract {
	public:
		using contract::contract;
		mockvoters(name receiver, name code, datastream<const char *> ds):
		contract(receiver, code, ds)
		{}

		//Main Actions
		ACTION dosomething(const name& some_name);

		//Notifications
		[[eosio::on_notify("*::transfer")]] void receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);


	private:

		//Functions
		void transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo);
};



