namespace voters_ns {
    struct [[eosio::table]] voter_info
    {
      eosio::name owner;
      eosio::name proxy;
      std::vector<eosio::name> producers;
      int64_t staked = 0;
      double unpaid_voteshare = 0;
      eosio::time_point unpaid_voteshare_last_updated;
      double unpaid_voteshare_change_rate;
      eosio::time_point last_claim_time;
      double last_vote_weight = 0;
      double proxied_vote_weight = 0;
      bool is_proxy = 0;
      uint32_t flags1 = 0;
      uint32_t reserved2 = 0;
      eosio::asset reserved3;

      uint64_t primary_key() const { return owner.value; }

      enum class flags1_fields : uint32_t {
        ram_managed = 1,
        net_managed = 2,
        cpu_managed = 4
      };

      EOSLIB_SERIALIZE(voter_info,
          (owner)(proxy)(producers)(staked)(unpaid_voteshare)(
              unpaid_voteshare_last_updated)(unpaid_voteshare_change_rate)(
              last_claim_time)(last_vote_weight)(proxied_vote_weight)(is_proxy)(
              flags1)(reserved2)(reserved3))
    };

    typedef eosio::multi_index<"voters"_n, voter_info> voters_table;
}