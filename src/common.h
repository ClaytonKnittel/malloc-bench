#pragma once

#include <concepts>
#include <cstddef>

namespace bench {

template <typename T>
concept MallocInterface = requires(size_t size, void* ptr) {
  { T::initialize_heap() } -> std::same_as<void>;
  { T::malloc(size) } -> std::convertible_to<void*>;
  { T::calloc(size, size) } -> std::convertible_to<void*>;
  { T::realloc(ptr, size) } -> std::convertible_to<void*>;
  { T::free(ptr) } -> std::same_as<void>;
};

}  // namespace bench
