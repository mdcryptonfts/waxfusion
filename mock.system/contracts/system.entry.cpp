#pragma once

#include "system.hpp"
#include "on_notify.cpp"

//contractName: mocksystem

static constexpr uint32_t refund_delay_sec = 3*24*3600;

uint64_t now(){
  return current_time_point().sec_since_epoch();
}

void mocksystem::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo){
    action(permission_level{ _self, "active"_n}, contract,"transfer"_n,std::tuple{ _self, user, amount_to_send, memo}).send();
    return;
}

ACTION mocksystem::claimgbmvote( const name& owner ){
    require_auth( owner );

    auto itr = voters_t.require_find( owner.value, "you arent staking anything" );

    //make sure staked_amount > 0
    check( itr->amount_staked.amount > 0, "you have 0 wax staked" );

    //make sure its been 24h since last claim
    check( now() - (60 * 60 * 24) >= itr->last_claim, "hasnt been 24h since your last claim" );

    //calculate a mock APR ( amount_staked * 0.8 / 365 );

    //amount_staked * 0.08 * seconds_staked / ( 60 * 60 * 24 * 365 )
    uint64_t seconds_per_year = 60 * 60 * 24 * 365;
    uint64_t seconds_staked = now() - itr->last_claim;
    double rewards = double(itr->amount_staked.amount) * 0.08 * double(seconds_staked) / double(seconds_per_year);

    check( rewards >= 1.0, "you have nothing to claim" );

    asset amount_to_send = asset( int64_t(rewards), WAX_SYMBOL );
    std::string memo_to_send = "|voter pay|" + owner.to_string() + "|";    

    voters_t.modify(itr, same_payer, [&](auto &_row){
        _row.payouts.push_back( amount_to_send );
        _row.last_claim = now();
    });



    transfer_tokens( "eosio.voters"_n, amount_to_send, WAX_CONTRACT, memo_to_send );
}

ACTION mocksystem::delegatebw( name from, name receiver,
                                  asset stake_net_quantity,
                                  asset stake_cpu_quantity, bool transfer )
{
    require_auth( from );
   asset zero_asset = asset( 0, WAX_SYMBOL );
   check( stake_cpu_quantity >= zero_asset, "must stake a positive amount" );
   check( stake_net_quantity >= zero_asset, "must stake a positive amount" );
   check( stake_net_quantity.amount + stake_cpu_quantity.amount > 0, "must stake a positive amount" );

   eosio::asset total_to_stake = stake_net_quantity + stake_cpu_quantity;

   //find the user in the voters table and change their amount_staked
   auto itr = voters_t.find( from.value );

   if( itr == voters_t.end() ){
    voters_t.emplace(from, [&](auto &_row){
        _row.voter = from;
        _row.amount_staked = total_to_stake;
        _row.last_claim = now();
        _row.amount_deposited = asset(0, WAX_SYMBOL);
    });
   } else {
    voters_t.modify(itr, same_payer, [&](auto &_row){
        _row.amount_staked += total_to_stake;
    });
   }

   //upsert the delbw table with this stake info
   del_bandwidth_table delbw_t = del_bandwidth_table( _self, from.value );

   auto bw_itr = delbw_t.find( receiver.value );

   if( bw_itr == delbw_t.end() ){
    delbw_t.emplace(from, [&](auto &_row){
        _row.from = from;
        _row.to = receiver;
        _row.net_weight = stake_net_quantity;
        _row.cpu_weight = stake_cpu_quantity;
    });
   } else {
    delbw_t.modify(bw_itr, same_payer, [&](auto &_row){
        _row.net_weight += stake_net_quantity;
        _row.cpu_weight += stake_cpu_quantity;
    });
   }

   action(permission_level{ _self, "active"_n}, _self,"requestwax"_n,std::tuple{ from, total_to_stake }).send();

} // delegatebw


ACTION mocksystem::initproducer()
{

    std::vector<eosio::name> prods = {
        "guild.waxdao"_n,
        "guild.a"_n,
        "guild.b"_n,
        "guild.c"_n,
        "guild.d"_n,
        "guild.e"_n,
        "guild.f"_n,
        "guild.g"_n,
        "guild.h"_n,
        "guild.i"_n,
        "guild.j"_n,
        "guild.k"_n,
        "guild.l"_n,
        "guild.m"_n,
        "guild.n"_n,
        "guild.o"_n,
        "guild.p"_n,
        "guild.q"_n,
        "guild.r"_n,
        "guild.s"_n,
        "guild.t"_n,
        "guild.u"_n,
        "guild.v"_n,
        "guild.w"_n,
        "guild.x"_n,
        "guild.y"_n,
        "guild.z"_n,
        "guild.aa"_n,
        "guild.bb"_n,
        "guild.cc"_n,
        "guild.dd"_n,
        "guild.ee"_n,
        "guild.ff"_n,
        "guild.gg"_n,
        "guild.hh"_n,
        "guild.ii"_n,
        "guild.jj"_n,
        "guild.kk"_n,
        "guild.ll"_n,
        "guild.mm"_n,
        "guild.nn"_n,    
        "guild.oo"_n,
        "guild.pp"_n,
        "guild.qq"_n                           
    };

    double vote_count = 333858757569420694.420;
    int count = 0;

    for( name p : prods ){

        prod_t.emplace(_self, [&](auto &_row){
            _row.owner = p;
            _row.total_votes = vote_count;
            _row.is_active = count >= prods.size() - 21 ? true : false;
        });

        vote_count *= 1.069420;
        count++;
    }
}

ACTION mocksystem::refund( const name owner ){
   require_auth( owner );

   refunds_table refunds_tbl( _self, owner.value );
   auto req = refunds_tbl.find( owner.value );
   check( req != refunds_tbl.end(), "refund request not found" );

   check( req->request_time + seconds(refund_delay_sec) <= current_time_point(),
          "refund is not available yet" );


   //transfer tokens back to owner with "unstake" memo
   //in the unit tests, need to make sure that after calling delbw,
   //we transfer the amount of WAX that was requested to this contract
   //with a "stake" memo, and then store the balance in the voters table
   asset amount_to_send = req->net_amount + req->cpu_amount;
   std::string memo_to_send = "|unstake|" + owner.to_string() + "|";
   transfer_tokens( "eosio.stake"_n , amount_to_send, WAX_CONTRACT, memo_to_send );

   refunds_tbl.erase( req );
}

/** since this is a mock system contract and we aren't adjust the user's
 *  WAX balance automatically when they stake CPU/NET,
 *  what we will do is create a requestwax action here
 *  when the user stakes, we will call this inline and notify them
 *  so they can react by sending us the wax,
 *  but still allow the other contracts to keep this logic
 *  abstracted from the main logic
 *  rather than adding additional code to make inline transfers, which
 *  would make things unclear between what code is meant for testing 
 *  and what code is meant for mainnet
 */

ACTION mocksystem::requestwax(const name& payer, const asset& wax_amount){
    require_auth( _self );
    require_recipient( payer );
}

ACTION mocksystem::undelegatebw( name from, name receiver,
                                    asset unstake_net_quantity, asset unstake_cpu_quantity )
{
    require_auth( from );
   asset zero_asset( 0, WAX_SYMBOL );
   check( unstake_cpu_quantity >= zero_asset, "must unstake a positive amount" );
   check( unstake_net_quantity >= zero_asset, "must unstake a positive amount" );
   check( unstake_cpu_quantity.amount + unstake_net_quantity.amount > 0, "must unstake a positive amount" );

   //find the row in the delbw table
   //if its empty now, delete it

   del_bandwidth_table delbw_t = del_bandwidth_table( _self, from.value );

   auto bw_itr = delbw_t.require_find( receiver.value, ("you are not staking any wax to " + (receiver).to_string() ).c_str() );

   asset current_net = bw_itr->net_weight;
   asset current_cpu = bw_itr->cpu_weight;

   check( current_net >= unstake_net_quantity, ("you only have " + current_net.to_string() + " staked to NET").c_str() );
   check( current_cpu >= unstake_cpu_quantity, ("you only have " + current_cpu.to_string() + " staked to CPU").c_str() );

   if( current_cpu == unstake_cpu_quantity && current_net == unstake_net_quantity ){
    bw_itr = delbw_t.erase( bw_itr );
   } else {
    delbw_t.modify(bw_itr, same_payer, [&](auto &_row){
        _row.cpu_weight -= unstake_cpu_quantity;
        _row.net_weight -= unstake_net_quantity;
    });
   }

   //upsert the refunds table (reset timer, increment wax amount)
   refunds_table refunds_t( _self, from.value );
   auto refund_itr = refunds_t.find( from.value );

   if( refund_itr == refunds_t.end() ){
        refunds_t.emplace(from, [&](auto &_row){
            _row.owner = from;
            _row.request_time = current_time_point();;
            _row.net_amount = unstake_net_quantity;
            _row.cpu_amount = unstake_cpu_quantity;
        });
   } else {
        refunds_t.modify(refund_itr, same_payer, [&](auto &_row){
            _row.request_time = current_time_point();;
            _row.net_amount += unstake_net_quantity;
            _row.cpu_amount += unstake_cpu_quantity;
        });
   }

   //require find and update the voters table
   auto voter_itr = voters_t.require_find( from.value, "no row found in the voters table" );
   asset total_to_unstake = unstake_cpu_quantity + unstake_net_quantity;
   check( total_to_unstake <= voter_itr->amount_staked, "trying to unstake more than you staked" );
   check( total_to_unstake <= voter_itr->amount_deposited, "trying to unstake more than you deposited" );

   voters_t.modify(voter_itr, same_payer, [&](auto &_row){
        _row.amount_staked -= total_to_unstake;
        _row.amount_deposited -= total_to_unstake;
   });
     

} // undelegatebw


ACTION mocksystem::voteproducer( const name voter_name, const name proxy, const std::vector<name>& producers ){
   require_auth( voter_name );
   check( producers.size() >= 16, "must vote for at least 16 producers" );

   for( eosio::name p : producers ){
        auto p_itr = prod_t.require_find( p.value, ( p.to_string() + " is not a producer" ).c_str() );
   }

   auto itr = voters_t.require_find( voter_name.value, "you havent staked any resources" );

   /** too lazy to write real logic for calculating staking payout logic since its not important
    *  so when claiming rewards, we will just take the amount_staked in the table
    *  and the last_claim and calculate based on that
    */

    voters_t.modify( itr, same_payer, [&](auto &_row){
        _row.producers = producers;
    });
}