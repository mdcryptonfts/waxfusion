#pragma once

//Numeric Limits
static constexpr int64_t MAX_ASSET_AMOUNT = 4611686018427387903;

//Symbols
static constexpr eosio::symbol WAX_SYMBOL = eosio::symbol("WAX", 8);

//Contract names
static constexpr eosio::name DAPP_CONTRACT = "dapp.fusion"_n;
static constexpr eosio::name WAX_CONTRACT = "eosio.token"_n;