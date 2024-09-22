#pragma once

#include <cstddef>
#include <cstdint>

namespace jsmalloc {
namespace twiddle {

inline uint64_t PtrValue(void* ptr) {
  return reinterpret_cast<uint8_t*>(ptr) - static_cast<uint8_t*>(nullptr);
}

template <typename T>
inline T* AddPtrOffset(void* ptr, int32_t offset_bytes) {
  return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(ptr) + offset_bytes);
}

template <class T, class M>
static inline constexpr ptrdiff_t OffsetOf(const M T::*member) {
  return reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<T*>(0)->*member));
}

template <class T, class M>
static inline constexpr T* OwnerOf(const M* ptr, const M T::*member) {
  return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) -
                              OffsetOf(member));
}

}  // namespace twiddle
}  // namespace jsmalloc
