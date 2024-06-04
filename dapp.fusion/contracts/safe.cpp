#pragma once

// auto rounding down
int64_t fusion::mulDiv(uint64_t a, uint64_t b, uint128_t denominator) {
  eosio::check(denominator != 0, "DivideByZeroException");

  uint128_t prod = uint128_t(a) * uint128_t(b);
  uint128_t result = prod / denominator;

  return safecast::safe_cast<int64_t>(result);
}


int64_t fusion::safeAddInt64(const int64_t& a, const int64_t& b) {
  int64_t sum;

  if (((b > 0) && (a > ( std::numeric_limits<int64_t>::max() - b ))) ||
      ((b < 0) && (a < ( std::numeric_limits<int64_t>::min() - b )))) {
    check(false, "int64 addition would result in overflow or underflow");
  } else {
    sum = a + b;
  }

  return sum;
}

uint64_t fusion::safeAddUInt64(const uint64_t& a, const uint64_t& b) {
  uint64_t sum;

  //precondition test
  if ( std::numeric_limits<uint64_t>::max() - a < b ) {
    check(false, "uint64_t addition would result in wrapping");
  } else {
    sum = a + b;
  }

  //postcondition test
  if (sum < a) {
    check(false, "uint64_t addition resulted in wrapping");
  }

  return sum;
}


uint128_t fusion::safeAddUInt128(const uint128_t& a, const uint128_t& b) {
  uint128_t sum;

  //precondition test
  if ( std::numeric_limits<uint128_t>::max() - a < b ) {
    check(false, "uint128_t addition would result in wrapping");
  } else {
    sum = a + b;
  }

  //postcondition test
  if (sum < a) {
    check(false, "uint128_t addition resulted in wrapping");
  }

  return sum;
}

int64_t fusion::safeDivInt64(const int64_t& a, const int64_t& b) {
  int64_t result;
  if ((b == 0) || ((a == std::numeric_limits<int64_t>::min()) && (b == -1))) {
    check( false, "int64_t division would result in over/underflow" );
  } else {
    result = a / b;
  }

  return result;
}

uint64_t fusion::safeDivUInt64(const uint64_t& a, const uint64_t& b) {
  uint64_t result;

  if ( b == 0 ) {
    check( false, "cant divide by 0" );
  }

  return a / b;
}

uint128_t fusion::safeDivUInt128(const uint128_t& a, const uint128_t& b) {
  uint128_t result;

  if ( b == 0 ) {
    check( false, "cant divide by 0" );
  }

  return a / b;
}

uint128_t fusion::safeMulUInt128(const uint128_t& a, const uint128_t& b) {
  uint128_t result;

  if (a > 0) {  /* a is positive */
    if (b > 0) {  /* a and b are positive */
      if (a > (std::numeric_limits<uint128_t>::max() / b)) {
        check(false, "uint128_t multiplication would result in over/underflow");
      }
    } else { /* a positive, b nonpositive */
      if (b < (std::numeric_limits<uint128_t>::min() / a)) {
        check(false, "uint128_t multiplication would result in over/underflow");
      }
    } /* a positive, b nonpositive */
  } else { /* a is nonpositive */
    if (b > 0) { /* a is nonpositive, b is positive */
      if (a < (std::numeric_limits<uint128_t>::min() / b)) {
        check(false, "uint128_t multiplication would result in over/underflow");
      }
    } else { /* a and b are nonpositive */
      if ( (a != 0) && (b < (std::numeric_limits<uint128_t>::max() / a))) {
        check(false, "uint128_t multiplication would result in over/underflow");
      }
    } /* End if a and b are nonpositive */
  } /* End if a is nonpositive */

  result = a * b;
  return result;

}

uint64_t fusion::safeMulUInt64(const uint64_t& a, const uint64_t& b) {
  uint64_t result;

  if (a > 0) {  /* a is positive */
    if (b > 0) {  /* a and b are positive */
      if (a > (std::numeric_limits<uint64_t>::max() / b)) {
        check(false, "uint64_t multiplication would result in over/underflow");
      }
    } else { /* a positive, b nonpositive */
      if (b < (std::numeric_limits<uint64_t>::min() / a)) {
        check(false, "uint64_t multiplication would result in over/underflow");
      }
    } /* a positive, b nonpositive */
  } else { /* a is nonpositive */
    if (b > 0) { /* a is nonpositive, b is positive */
      if (a < (std::numeric_limits<uint64_t>::min() / b)) {
        check(false, "uint64_t multiplication would result in over/underflow");
      }
    } else { /* a and b are nonpositive */
      if ( (a != 0) && (b < (std::numeric_limits<uint64_t>::max() / a))) {
        check(false, "uint64_t multiplication would result in over/underflow");
      }
    } /* End if a and b are nonpositive */
  } /* End if a is nonpositive */

  result = a * b;
  return result;
}

int64_t fusion::safeSubInt64(const int64_t& a, const int64_t& b) {
  int64_t diff;
  if ((b > 0 && a < std::numeric_limits<int64_t>::min() + b) ||
      (b < 0 && a > std::numeric_limits<int64_t>::max() + b)) {
    check(false, "subtraction would result in overflow or underflow");
  } else {
    diff = a - b;
  }

  return diff;
}

uint64_t fusion::safeSubUInt64(const uint64_t& a, const uint64_t& b) {
  uint64_t diff;
  if ((b > 0 && a < std::numeric_limits<uint64_t>::min() + b) ||
      (b < 0 && a > std::numeric_limits<uint64_t>::max() + b)) {
    check(false, "subtraction would result in overflow or underflow");
  } else {
    diff = a - b;
  }

  return diff;
}