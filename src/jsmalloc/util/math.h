#pragma once

#include <cstring>

namespace jsmalloc {
namespace math {

/** Rounds size up to the nearest 16 byte boundary. */
constexpr size_t round_16b(size_t size) {
  return (size + 0xf) & ~0xf;
}

template <typename T>
constexpr T div_ceil(T n, T d) {
  return (n + d - 1) / d;
}

}  // namespace math
}  // namespace jsmalloc
