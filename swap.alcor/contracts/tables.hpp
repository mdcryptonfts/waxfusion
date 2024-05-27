#pragma once

  struct CurrSlotS {
    uint128_t     sqrtPriceX64;
    int32_t       tick;
    uint32_t      lastObservationTimestamp;
    uint32_t      currentObservationNum;
    uint32_t      maxObservationNum;
  };


  struct[[eosio::table, eosio::contract(CONTRACT_NAME)]] incentives {
    uint64_t                  id;
    eosio::name               creator;
    uint64_t                  poolId;
    eosio::extended_asset     reward;
    uint32_t                  periodFinish;
    uint32_t                  rewardsDuration;
    uint128_t                 rewardRateE18;
    uint128_t                 rewardPerTokenStored;
    uint64_t                  totalStakingWeight;
    uint32_t                  lastUpdateTime;
    uint32_t                  numberOfStakes;

    uint64_t  primary_key()const { return id; }
  };
  typedef eosio::multi_index< "incentives"_n, incentives> incentives_table;


  struct[[eosio::table, eosio::contract(CONTRACT_NAME)]] pools_struct {
    uint64_t                  id;
    bool                      active;
    eosio::extended_asset     tokenA;
    eosio::extended_asset     tokenB;
    uint32_t                  fee;
    uint8_t                   feeProtocol;
    int32_t                   tickSpacing;
    uint64_t                  maxLiquidityPerTick;
    CurrSlotS                 currSlot;
    uint64_t                  feeGrowthGlobalAX64;
    uint64_t                  feeGrowthGlobalBX64;
    eosio::asset              protocolFeeA;
    eosio::asset              protocolFeeB;
    uint64_t                  liquidity;

    uint64_t  primary_key()const { return id; }
  };
  typedef eosio::multi_index< "pools"_n, pools_struct> pools_table;
