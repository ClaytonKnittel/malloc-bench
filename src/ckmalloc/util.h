#pragma once

#include <cstdlib>
#include <iostream>
#include <optional>
#include <type_traits>

#ifdef NDEBUG

#define CK_ASSERT(cond) __builtin_assume(cond)

#else

#define CK_ASSERT(cond)                                                     \
  do {                                                                      \
    /* NOLINTNEXTLINE(readability-simplify-boolean-expr) */                 \
    if (!(cond)) {                                                          \
      std::cerr << __FILE__ ":" << __LINE__ << ": Condition failed: " #cond \
                << std::endl;                                               \
      std::abort();                                                         \
    }                                                                       \
  } while (0)

#endif

#define CK_UNREACHABLE() __builtin_unreachable()

namespace ckmalloc {

template <typename T>
std::optional<T> OptionalOr(std::optional<T>&& primary,
                            std::optional<T>&& secondary) {
  return primary.has_value() ? std::move(primary) : std::move(secondary);
}

template <typename T, typename Fn>
std::optional<T> OptionalOrElse(std::optional<T>&& primary, Fn&& fn) {
  if (primary.has_value()) {
    return std::move(primary.value());
  }

  return fn();
}

template <typename T>
requires std::is_integral_v<T>
constexpr bool IsAligned(T val, T alignment) {
  CK_ASSERT((alignment & (alignment - 1)) == 0);
  return (val & ~(alignment - 1)) == 0;
}

template <typename T>
requires std::is_integral_v<T>
constexpr T AlignUp(T val, T alignment) {
  CK_ASSERT((alignment & (alignment - 1)) == 0);
  return (val + alignment - 1) & ~(alignment - 1);
}

}  // namespace ckmalloc
