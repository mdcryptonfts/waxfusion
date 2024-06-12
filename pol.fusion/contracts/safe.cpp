#pragma once

// auto rounding down
int64_t polcontract::mulDiv(const uint64_t& a, const uint64_t& b, const uint128_t& denominator) {
  uint128_t prod = safecast::mul(uint128_t(a), uint128_t(b));
  uint128_t result = safecast::div(prod, denominator);

  return safecast::safe_cast<int64_t>(result);
}

uint128_t polcontract::mulDiv128(const uint128_t& a, const uint128_t& b, const uint128_t& denominator) {
  check(denominator != 0, "can not divide by 0");

  uint256_t prod = uint256_t(a) * uint256_t(b);
  uint256_t result = prod / uint256_t(denominator);
  check(result <= std::numeric_limits<uint128_t>::max(), "mulDiv resulted in overflow");

  return uint128_t(result);
}