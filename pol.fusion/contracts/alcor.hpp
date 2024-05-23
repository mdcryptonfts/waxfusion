#pragma once

namespace alcor_contract {
  struct CurrSlotS {
    uint128_t     sqrtPriceX64;
    int32_t       tick;
    uint32_t      lastObservationTimestamp;
    uint32_t      currentObservationNum;
    uint32_t      maxObservationNum;
  };

  struct[[eosio::table]] pools_struct {
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
}