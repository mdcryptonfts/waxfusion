#pragma once


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] delegated_bandwidth {
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


inline eosio::block_signing_authority convert_to_block_signing_authority( const eosio::public_key& producer_key ) {
  return eosio::block_signing_authority_v0{ .threshold = 1, .keys = {{producer_key, 1}} };
} 

struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] producer_info {
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
     if( producer_authority.has_value() ) {
        bool zero_threshold = std::visit( [](auto&& auth ) -> bool {
           return (auth.threshold == 0);
        }, *producer_authority );
        // zero_threshold could be true despite the validation done in regproducer2 because the v1.9.0 eosio.system
        // contract has a bug which may have modified the producer table such that the producer_authority field
        // contains a default constructed eosio::block_signing_authority (which has a 0 threshold and so is invalid).
        if( !zero_threshold ) return *producer_authority;
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

     if( !t.producer_authority.has_value() ) return ds;

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


struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] refund_request {
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

struct [[eosio::table, eosio::contract(CONTRACT_NAME)]] voters {
  eosio::name                 voter;
  eosio::asset                amount_staked;
  uint64_t                    last_claim;
  std::vector<eosio::name>    producers;
  eosio::asset                amount_deposited;
  std::vector<eosio::asset>   payouts;

  uint64_t  primary_key()const { return voter.value; }

};

typedef eosio::multi_index< "voters"_n, voters > voters_table;                         