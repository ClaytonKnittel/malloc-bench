#pragma once

#include <cstring>

namespace jsmalloc {
namespace math {

/** Rounds size up to the nearest 16 byte boundary. */
constexpr size_t round_16b(size_t size) {
  return (size + 0xf) & ~0xf;
}

}  // namespace math
}  // namespace jsmalloc
