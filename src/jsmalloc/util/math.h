#pragma once

#include <bit>
#include <cstring>
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace math {

constexpr size_t round_up_pow2(size_t n, size_t d) {
  DCHECK_EQ(std::popcount(d), 1);
  size_t m = d - 1;
  return (n + m) & ~m;
}

/** Rounds size up to the nearest 16 byte boundary. */
constexpr size_t round_16b(size_t size) {
  return round_up_pow2(size, 16);
}

template <typename T>
constexpr T div_ceil(T n, T d) {
  return (n + d - 1) / d;
}

}  // namespace math
}  // namespace jsmalloc
