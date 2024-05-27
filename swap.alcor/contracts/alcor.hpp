#pragma once
#define CONTRACT_NAME "alcor"

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


using namespace eosio;


CONTRACT alcor : public contract {
	public:
		using contract::contract;
		alcor(name receiver, name code, datastream<const char *> ds):
		contract(receiver, code, ds)
		{}

		//Main Actions
		ACTION initunittest(const eosio::asset& wax_amount, const eosio::asset& lswax_amount);
		ACTION newincentive(const name& creator, const uint64_t& poolId, const extended_asset& rewardToken, const uint32_t& duration);

		//Notifications
		[[eosio::on_notify("*::transfer")]] void receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);


	private:

		//Multi Index Tables
		incentives_table incentives_t = incentives_table( _self, _self.value );
		pools_table pools_t = pools_table( _self, _self.value);

};



