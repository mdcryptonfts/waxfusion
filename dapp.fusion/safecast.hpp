#pragma once

/** safecast namespace
 *  used for converting larger types to smaller types without having to write redundant checks before static casting
 *  for example when doing uint128_t math and then converting the result to int64_t
 *  instead of having to check every time if the result is larger than the max int64_t value, 
 *  we can just cast safely using `safe_cast<int64_t>(result)`
 */

namespace safecast {

    template<typename T, typename S>
    T safe_cast(S value);

    //specialization to convert uint128_t to int64_t
    template<>
    int64_t safe_cast<int64_t, uint128_t>(uint128_t value) {
        eosio::check(value <= static_cast<uint128_t>(std::numeric_limits<int64_t>::max()), "Overflow detected in safe_cast");
        return static_cast<int64_t>(value);
    }

    //specialization to convert uint64_t to int64_t
    template<>
    int64_t safe_cast<int64_t, uint64_t>(uint64_t value) {
        eosio::check(value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()), "Overflow detected in safe_cast");
        return static_cast<int64_t>(value);
    }

}
