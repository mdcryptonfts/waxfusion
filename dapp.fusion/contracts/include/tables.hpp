#pragma once

namespace alcor_contract {

  struct CurrSlotS {
    uint128_t     sqrtPriceX64;
    int32_t       tick;
    uint32_t      lastObservationTimestamp;
    uint32_t      currentObservationNum;
    uint32_t      maxObservationNum;
  };


  struct[[eosio::table]] incentives {
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

  typedef eosio::multi_index< "incentives"_n, incentives> incentives_table;
  typedef eosio::multi_index< "pools"_n, pools_struct> pools_table;
  
}

namespace pol_contract {

  struct [[eosio::table]] state3 {
    eosio::asset      wax_available_for_rentals;
    uint64_t          next_day_end_time;
    eosio::asset      cost_to_rent_1_wax;
    uint64_t          last_vote_time;
    eosio::asset      wax_bucket;
    eosio::asset      lswax_bucket;
    uint64_t          last_liquidity_addition_time;
    eosio::asset      wax_allocated_to_rentals;
    eosio::asset      pending_refunds;
    uint64_t          last_rebalance_time;

    EOSLIB_SERIALIZE(state3,
                     (wax_available_for_rentals)
                     (next_day_end_time)
                     (cost_to_rent_1_wax)
                     (last_vote_time)
                     (wax_bucket)
                     (lswax_bucket)
                     (last_liquidity_addition_time)
                     (wax_allocated_to_rentals)
                     (pending_refunds)
                     (last_rebalance_time)
                    )
  };
  using state_singleton_3 = eosio::singleton<"state3"_n, state3>;

}

struct [[eosio::table]] account {
  eosio::asset    balance;

  uint64_t primary_key()const { return balance.symbol.code().raw(); }
};
typedef eosio::multi_index< "accounts"_n, account > accounts;

struct [[eosio::table]] stat {
  eosio::asset      supply;
  eosio::asset      max_supply;
  eosio::name       issuer;

  uint64_t primary_key()const { return supply.symbol.code().raw(); }
};
typedef eosio::multi_index< "stat"_n, stat > stat_table;


// Every user 'from' has a scope/table that uses every recipient 'to' as the primary key.
struct [[eosio::table]] delegated_bandwidth {
  eosio::name          from;
  eosio::name          to;
  eosio::asset         net_weight;
  eosio::asset         cpu_weight;

  bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0; }
  uint64_t  primary_key()const { return to.value; }

  // explicit serialization macro is not necessary, used here only to improve compilation time
  EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight) )

};

typedef eosio::multi_index< "delband"_n, delegated_bandwidth > del_bandwidth_table;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] epochs {
  uint64_t          start_time;
  uint64_t          time_to_unstake;
  eosio::name       cpu_wallet;
  eosio::asset      wax_bucket;
  eosio::asset      wax_to_refund;
  uint64_t          redemption_period_start_time;
  uint64_t          redemption_period_end_time;
  eosio::asset      total_cpu_funds_returned;
  eosio::asset      total_added_to_redemption_bucket;

  uint64_t primary_key() const { return start_time; }
};
using epochs_table = eosio::multi_index<"epochs"_n, epochs>;

/**
 * The incentive_ids table stores the ids of incentives created on Alcor.
 * 
 * NOTE: The `lpfarms` table stores most of the data related to farms.
 * The idea to also store incentive ids came after the contract was already
 * on mainnet, so rather than modifying the existing table and potentially 
 * breaking other contracts/indexers, it was decided to create a second 
 * table as an extension instead.
 * 
 * Scoped by _self
 */ 

struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] incentive_ids {
  uint64_t  pool_id;
  uint64_t  incentive_id;                

  uint64_t primary_key() const { return pool_id; }
};
using incentive_ids_table = eosio::multi_index<"incentiveids"_n, incentive_ids>;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] lpfarms {
  uint64_t                poolId;
  eosio::symbol           symbol_to_incentivize;
  eosio::name             contract_to_incentivize;
  uint64_t                percent_share_1e6;   // percentage of ecosystem fund, not percentage of total revenue

  uint64_t primary_key() const { return poolId; }
};
using lpfarms_table = eosio::multi_index<"lpfarms"_n, lpfarms>;

inline eosio::block_signing_authority convert_to_block_signing_authority( const eosio::public_key& producer_key ) {
  return eosio::block_signing_authority_v0{ .threshold = 1, .keys = {{producer_key, 1}} };
}

struct [[eosio::table]] producer_info {
  eosio::name                                              owner;
  double                                                   total_votes = 0;
  eosio::public_key                                        producer_key; /// a packed public key object
  bool                                                     is_active = true;
  std::string                                              url;
  uint32_t                                                 unpaid_blocks = 0;
  eosio::time_point                                        last_claim_time;
  uint16_t                                                 location = 0;
  eosio::binary_extension<eosio::block_signing_authority>  producer_authority; // added in version 1.9.0

  uint64_t primary_key()const { return owner.value;                             }
  double   by_votes()const    { return is_active ? -total_votes : total_votes;  }
  bool     active()const      { return is_active;                               }
  void     deactivate()       { producer_key = eosio::public_key(); producer_authority.reset(); is_active = false; }

  eosio::block_signing_authority get_producer_authority()const {
    if ( producer_authority.has_value() ) {
      bool zero_threshold = std::visit( [](auto && auth ) -> bool {
        return (auth.threshold == 0);
      }, *producer_authority );
      // zero_threshold could be true despite the validation done in regproducer2 because the v1.9.0 eosio.system
      // contract has a bug which may have modified the producer table such that the producer_authority field
      // contains a default constructed eosio::block_signing_authority (which has a 0 threshold and so is invalid).
      if ( !zero_threshold ) return *producer_authority;
    }
    return convert_to_block_signing_authority( producer_key );
  }

  // The unregprod and claimrewards actions modify unrelated fields of the producers table and under the default
  // serialization behavior they would increase the size of the serialized table if the producer_authority field
  // was not already present. This is acceptable (though not necessarily desired) because those two actions require
  // the authority of the producer who pays for the table rows.
  // However, the rmvproducer action and the onblock transaction would also modify the producer table in a similar
  // way and increasing its serialized size is not acceptable in that context.
  // So, a custom serialization is defined to handle the binary_extension producer_authority
  // field in the desired way. (Note: v1.9.0 did not have this custom serialization behavior.)

  template<typename DataStream>
  friend DataStream& operator << ( DataStream& ds, const producer_info& t ) {
    ds << t.owner
       << t.total_votes
       << t.producer_key
       << t.is_active
       << t.url
       << t.unpaid_blocks
       << t.last_claim_time
       << t.location;

    if ( !t.producer_authority.has_value() ) return ds;

    return ds << t.producer_authority;
  }

  template<typename DataStream>
  friend DataStream& operator >> ( DataStream& ds, producer_info& t ) {
    return ds >> t.owner
           >> t.total_votes
           >> t.producer_key
           >> t.is_active
           >> t.url
           >> t.unpaid_blocks
           >> t.last_claim_time
           >> t.location
           >> t.producer_authority;
  }
};
typedef eosio::multi_index< "producers"_n, producer_info,
eosio::indexed_by<"prototalvote"_n, eosio::const_mem_fun<producer_info, double, &producer_info::by_votes>  >
> producers_table;


// Scoped by user
struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] redeem_requests {
  uint64_t        epoch_id;
  eosio::asset    wax_amount_requested;

  uint64_t primary_key() const { return epoch_id; }
};
using requests_tbl = eosio::multi_index<"rdmrequests"_n, redeem_requests
                     >;


struct [[eosio::table]] refund_request {
  eosio::name             owner;
  eosio::time_point_sec   request_time;
  eosio::asset            net_amount;
  eosio::asset            cpu_amount;

  bool is_empty()const { return net_amount.amount == 0 && cpu_amount.amount == 0; }
  uint64_t  primary_key()const { return owner.value; }

  // explicit serialization macro is not necessary, used here only to improve compilation time
  EOSLIB_SERIALIZE( refund_request, (owner)(request_time)(net_amount)(cpu_amount) )
};
typedef eosio::multi_index< "refunds"_n, refund_request >      refunds_table;


/**
* total bytes for a row is 560, except for the initial row which was 896
* scoped by epoch ID. contract pays ram and removes rows after epoch ends
*/

struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] renters {
  uint64_t      ID;
  eosio::name   renter;
  eosio::name   rent_to_account;
  eosio::asset  amount_staked;

  uint64_t primary_key() const { return ID; }
  uint64_t second_key() const { return renter.value; }
  uint128_t by_from_to_combo() const { return mix64to128( renter.value, rent_to_account.value ); }
};
using renters_table = eosio::multi_index<"renters"_n, renters,
      eosio::indexed_by<"renter"_n, eosio::const_mem_fun<renters, uint64_t, &renters::second_key>>,
      eosio::indexed_by<"fromtocombo"_n, eosio::const_mem_fun<renters, uint128_t, &renters::by_from_to_combo>>
      >;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] rewards {
  uint64_t        periodStart;
  uint64_t        periodFinish;
  uint128_t       rewardRate;
  uint64_t        rewardsDuration;
  uint64_t        lastUpdateTime;
  uint128_t       rewardPerTokenStored;
  eosio::asset    rewardPool;
  uint128_t       totalSupply;
  eosio::asset    totalRewardsPaidOut;


  EOSLIB_SERIALIZE(rewards, (periodStart)
                   (periodFinish)
                   (rewardRate)
                   (rewardsDuration)
                   (lastUpdateTime)
                   (rewardPerTokenStored)
                   (rewardPool)
                   (totalSupply)
                   (totalRewardsPaidOut)
                  )
};
using rewards_singleton = eosio::singleton<"rewards"_n, rewards>;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] stakers {
  eosio::name     wallet;
  eosio::asset    swax_balance;
  uint64_t        last_update;
  eosio::asset    claimable_wax;
  uint128_t       userRewardPerTokenPaid;

  uint64_t primary_key() const { return wallet.value; }
};
using staker_table = eosio::multi_index< "stakers"_n, stakers >;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] top21 {
  std::vector<eosio::name>    block_producers;
  uint64_t                    last_update;

  EOSLIB_SERIALIZE(top21, (block_producers)(last_update))
};
using top21_singleton = eosio::singleton<"top21"_n, top21>;


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] version {
  std::string     version_id;
  std::string     changelog_url;

  EOSLIB_SERIALIZE(version, (version_id)(changelog_url))
};
using version_singleton = eosio::singleton< "version"_n, version >;