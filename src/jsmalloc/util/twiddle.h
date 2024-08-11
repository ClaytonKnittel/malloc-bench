
#include <bit>
#include <cstdint>

#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace twiddle {

constexpr uint32_t BitMask(uint32_t start, uint32_t end) {
  return ((1 << (end - start)) - 1) << start;
}

constexpr uint32_t SetBits(uint32_t dst, uint32_t src, uint32_t start,
                           uint32_t end) {
  DCHECK_EQ((src & BitMask(0, end - start)), src);
  dst &= ~BitMask(start, end);
  dst |= src << start;
  return dst;
}

constexpr uint32_t GetBits(uint32_t src, uint32_t start, uint32_t end) {
  src >>= start;
  src &= BitMask(0, end - start);
  return src;
}

inline uint64_t PtrValue(void* ptr) {
  return reinterpret_cast<uint8_t*>(ptr) - static_cast<uint8_t*>(nullptr);
}

template <typename T>
inline T* AddPtrOffset(void* ptr, int32_t offset_bytes) {
  return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(ptr) + offset_bytes);
}

}  // namespace twiddle
}  // namespace jsmalloc
