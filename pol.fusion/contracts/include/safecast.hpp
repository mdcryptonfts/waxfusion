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

    //specialization to convert uint128_t to uint64_t
    template<>
    uint64_t safe_cast<uint64_t, uint128_t>(uint128_t value) {
        eosio::check(value <= static_cast<uint128_t>(std::numeric_limits<uint64_t>::max()), "Overflow detected in safe_cast");
        return static_cast<uint64_t>(value);
    }    

    //specialization to convert uint64_t to int64_t
    template<>
    int64_t safe_cast<int64_t, uint64_t>(uint64_t value) {
        eosio::check(value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()), "Overflow detected in safe_cast");
        return static_cast<int64_t>(value);
    }

    template<typename T>
    T add(T a, T b){
        T sum;
        if constexpr (std::is_signed<T>::value) {
            //Handle signed integers
            if ( ((b > 0) && (a > ( std::numeric_limits<T>::max() - b )) ) ||
              ((b < 0) && (a < ( std::numeric_limits<T>::min() - b )))) {
                eosio::check(false, "addition would result in overflow or underflow");
            } else {
                sum = a + b;
            }

            return sum;

        } else {
            //Handle unsigned integers
            //precondition test
            if ( std::numeric_limits<T>::max() - a < b ) {
                eosio::check(false, "addition would result in wrapping");
            } else {
                sum = a + b;
            }

            //postcondition test
            if (sum < a) {
                eosio::check(false, "addition resulted in wrapping");
            }

            return sum;
        }        
    }

    template<typename T>
    T div(T a, T b){
      if ((b == 0) || ((a == std::numeric_limits<T>::min()) && (b == -1))) {
        eosio::check( false, "division would result in over/underflow" );
      } else {
        return a / b;
      }     
    }

    template<typename T>
    T mul(T a, T b){

      T result;

      if (a > 0) {  /* a is positive */
        if (b > 0) {  /* a and b are positive */
          if (a > (std::numeric_limits<T>::max() / b)) {
            eosio::check(false, "multiplication would result in over/underflow");
          }
        } else { /* a positive, b nonpositive */
          if (b < (std::numeric_limits<T>::min() / a)) {
            eosio::check(false, "multiplication would result in over/underflow");
          }
        } /* a positive, b nonpositive */
      } else { /* a is nonpositive */
        if (b > 0) { /* a is nonpositive, b is positive */
          if (a < (std::numeric_limits<T>::min() / b)) {
            eosio::check(false, "multiplication would result in over/underflow");
          }
        } else { /* a and b are nonpositive */
          if ( (a != 0) && (b < (std::numeric_limits<T>::max() / a))) {
            eosio::check(false, "multiplication would result in over/underflow");
          }
        } /* End if a and b are nonpositive */
      } /* End if a is nonpositive */

      result = a * b;
      return result;

    }

    template<typename T>
    T sub(T a, T b){
        T diff;
        if ( (b > 0 && a < std::numeric_limits<T>::min() + b ) ||
             (b < 0 && a > std::numeric_limits<T>::max() + b) ) 
        {
            eosio::check(false, "subtraction would result in overflow or underflow");
        } else {
            diff = a - b;
        }

        return diff;        
    }    

}
