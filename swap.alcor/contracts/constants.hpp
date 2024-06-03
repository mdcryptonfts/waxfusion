#pragma once

static constexpr eosio::symbol WAX_SYMBOL = eosio::symbol("WAX", 8);
static constexpr eosio::symbol LSWAX_SYMBOL = eosio::symbol("LSWAX", 8);

//Contract names
static constexpr eosio::name TOKEN_CONTRACT = "token.fusion"_n;
static constexpr eosio::name WAX_CONTRACT = "eosio.token"_n;

//Numeric
const uint128_t TWO_POW_64 = uint128_t(1) << 64;
const uint128_t SQRT_64_1_TO_1 = uint128_t(18446744073709552) * 1000;
static constexpr uint128_t SCALE_FACTOR_1E1 = 10;
static constexpr uint128_t SCALE_FACTOR_1E2 = 100;
static constexpr uint128_t SCALE_FACTOR_1E3 = 1000;
static constexpr uint128_t SCALE_FACTOR_1E4 = 10000;
static constexpr uint128_t SCALE_FACTOR_1E5 = 100000;
static constexpr uint128_t SCALE_FACTOR_1E6 = 1000000;
static constexpr uint128_t SCALE_FACTOR_1E7 = 10000000;
static constexpr uint128_t SCALE_FACTOR_1E8 = 100000000;
static constexpr uint128_t SCALE_FACTOR_1E9 = 1000000000;
static constexpr uint128_t SCALE_FACTOR_1E10 = 10000000000;
static constexpr uint128_t SCALE_FACTOR_1E11 = 100000000000;
static constexpr uint128_t SCALE_FACTOR_1E12 = 1000000000000;
static constexpr uint128_t SCALE_FACTOR_1E13 = 10000000000000;
static constexpr uint128_t SCALE_FACTOR_1E14 = 100000000000000;
static constexpr uint128_t SCALE_FACTOR_1E15 = 1000000000000000;
static constexpr uint128_t SCALE_FACTOR_1E16 = 10000000000000000;
static constexpr uint128_t SCALE_FACTOR_1E17 = 100000000000000000;
static constexpr uint128_t SCALE_FACTOR_1E18 = 1000000000000000000;
static constexpr uint128_t SCALE_FACTOR_1E19 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E1;
static constexpr uint128_t SCALE_FACTOR_1E20 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E2;
static constexpr uint128_t SCALE_FACTOR_1E21 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E3;
static constexpr uint128_t SCALE_FACTOR_1E22 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E4;
static constexpr uint128_t SCALE_FACTOR_1E23 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E5;
static constexpr uint128_t SCALE_FACTOR_1E24 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E6;
static constexpr uint128_t SCALE_FACTOR_1E25 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E7;
static constexpr uint128_t SCALE_FACTOR_1E26 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E8;
static constexpr uint128_t SCALE_FACTOR_1E27 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E9;
static constexpr uint128_t SCALE_FACTOR_1E28 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E10;
static constexpr uint128_t SCALE_FACTOR_1E29 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E11;
static constexpr uint128_t SCALE_FACTOR_1E30 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E12;
static constexpr uint128_t SCALE_FACTOR_1E31 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E13;
static constexpr uint128_t SCALE_FACTOR_1E32 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E14;
static constexpr uint128_t SCALE_FACTOR_1E33 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E15;
static constexpr uint128_t SCALE_FACTOR_1E34 = SCALE_FACTOR_1E18 * SCALE_FACTOR_1E16;