#pragma once
#define CONTRACT_NAME "mocksystem"

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
#include <eosio/binary_extension.hpp>
#include <eosio/producer_schedule.hpp>
#include <cmath>
#include "tables.hpp"
#include "constants.hpp"


using namespace eosio;


CONTRACT mocksystem : public contract {
	public:
		using contract::contract;
		mocksystem(name receiver, name code, datastream<const char *> ds):
		contract(receiver, code, ds)
		{}

		//Main Actions
		ACTION claimgbmvote( const name& owner );
		ACTION delegatebw( name from, name receiver,
                                  asset stake_net_quantity,
                                  asset stake_cpu_quantity, bool transfer );	
		ACTION initproducer();
		ACTION refund( const name owner );
		ACTION requestwax(const name& payer, const asset& wax_amount);
		ACTION undelegatebw( name from, name receiver,
                                    asset unstake_net_quantity, asset unstake_cpu_quantity );
		ACTION voteproducer( const name voter_name, const name proxy, const std::vector<name>& producers );

		//Notifications
		[[eosio::on_notify("eosio.token::transfer")]] void receive_token_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);


	private:

		//Multi Index Tables
		producers_table prod_t = producers_table( _self, _self.value );
		voters_table voters_t = voters_table( _self, _self.value );

		//Functions
		void transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo);
};



