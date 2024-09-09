#pragma once

#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <memory>

namespace jsmalloc {

/** A type that can be manually allocated. */
template <typename T, typename... Args>
concept AllocableT = requires(void* allocated_region, Args... args) {
  { T::RequiredSize(args...) } -> std::same_as<size_t>;
  { T::Init(allocated_region, args...) } -> std::same_as<T*>;
};

/**
 * Implements AllocableT for constructable types with statically known sizes.
 */
template <typename T, typename... Args>
class DefaultAllocable {
 public:
  static constexpr size_t RequiredSize(Args...) {
    return sizeof(T);
  }

  static T* Init(void* ptr, Args... args) {
    return new (ptr) T(args...);
  }
};

/** Creates an AllocableT type on the heap. */
template <typename T, typename... Args>
requires AllocableT<T, Args...>
std::unique_ptr<T> MakeAllocable(Args... args) {
  void* ptr = malloc(T::RequiredSize(args...));
  if (ptr == nullptr) {
    return std::unique_ptr<T>{};
  }
  return std::unique_ptr<T>(T::Init(ptr, args...));
}

}  // namespace jsmalloc
