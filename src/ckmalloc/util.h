#pragma once

#include <cstdlib>
#include <iostream>
#include <optional>

#ifdef NDEBUG

#define CK_ASSERT(cond)

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

namespace ckmalloc {

template <typename T>
std::optional<T> OptionalOr(std::optional<T>&& primary,
                            std::optional<T>&& secondary) {
  return primary.has_value() ? std::move(primary) : std::move(secondary);
}

}  // namespace ckmalloc
