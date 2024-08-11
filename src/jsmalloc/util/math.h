#pragma once

#include <cstdint>
#include <cstring>

namespace jsmalloc {
namespace math {

/** Rounds size up to the nearest 16 byte boundary. */
constexpr size_t round_16b(size_t size) {
  return (size + 0xf) & ~0xf;
}

/** Returns a mask of all ones if start<=n<end, or 0 otherwise. */
constexpr uint32_t case_mask(uint32_t n, uint32_t start, uint32_t end) {
  return 0 - static_cast<uint32_t>((start <= n) && (n < end));
}

/**
 * Returns the bucket that `n` belongs to.
 *
 * Start is inclusive, end is exclusive.
 * ——————————————————————
 * | bucket | start-end |
 * |      0 |      0-16 |
 * |      1 |     16-32 |
 * |      2 |     32-48 |
 * |      3 |     48-64 |
 * |      4 |     64-80 |
 * |      5 |     80-96 |
 * |      6 |    96-112 |
 * |      7 |   112-128 |
 * |      8 |   128-160 |
 * |      9 |   160-192 |
 * |     10 |   192-224 |
 * |     11 |   224-256 |
 * ——————————————————————
 */
constexpr uint32_t approximate_log16(uint32_t n) {
  uint32_t answer = 0;
  answer |= (n / 16) & case_mask(n, 0, 128);
  answer |= (8 + (n - 128) / 32) & case_mask(n, 128, 256);

  // To support n >= 256, uncomment the following lines:
  // answer |= (12 + (n - 256) / 64) & case_mask(n, 256, 512);
  // answer |= (16 + (n - 512) / 128) & case_mask(n, 512, 1024);

  return answer;
}

}  // namespace math
}  // namespace jsmalloc
