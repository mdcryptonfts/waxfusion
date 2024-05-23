#pragma once

#include "voters.hpp"
#include "on_notify.cpp"

//contractName: mockvoters

void mockvoters::transfer_tokens(const name& user, const asset& amount_to_send, const name& contract, const std::string& memo){
    action(permission_level{ _self, "active"_n}, contract,"transfer"_n,std::tuple{ _self, user, amount_to_send, memo}).send();
    return;
}

/** sometimes compilers throw an error if you have a contract
 *  that only has a notification handler and no actions
 *  so this is here just to avoid compilation errors
 */
ACTION mockvoters::dosomething(const name& some_name){
    require_auth( some_name );
    if(some_name != "eosio"_n ){
        check(false, "doing something");
    } else {
        check(false, "not doing something");
    }

}
