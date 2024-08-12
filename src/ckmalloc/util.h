#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <type_traits>

#ifdef NDEBUG

#define CK_ASSERT_MSG(cond, message) __builtin_assume(cond)

#else

#define CK_ASSERT_MSG(cond, message)                                        \
  do {                                                                      \
    /* NOLINTNEXTLINE(readability-simplify-boolean-expr) */                 \
    if (!(cond)) {                                                          \
      std::cerr                                                             \
          << __FILE__ ":" << __LINE__ << ": Condition failed: " #cond ", "  \
          << message /* NOLINT(bugprone-macro-parentheses) */ << std::endl; \
      std::abort();                                                         \
    }                                                                       \
  } while (0)

#endif

#define CK_ASSERT_INFIX(a, b, op, neg) \
  CK_ASSERT_MSG((a) op(b), (a) << (" " #neg " ") << (b))

#define CK_ASSERT_EQ(a, b) CK_ASSERT_INFIX(a, b, ==, !=)
#define CK_ASSERT_NE(a, b) CK_ASSERT_INFIX(a, b, !=, ==)
#define CK_ASSERT_LT(a, b) CK_ASSERT_INFIX(a, b, <, >=)
#define CK_ASSERT_LE(a, b) CK_ASSERT_INFIX(a, b, <=, >)
#define CK_ASSERT_GT(a, b) CK_ASSERT_INFIX(a, b, >, <=)
#define CK_ASSERT_GE(a, b) CK_ASSERT_INFIX(a, b, >=, <)
#define CK_ASSERT_TRUE(a)  CK_ASSERT_EQ(a, true)
#define CK_ASSERT_FALSE(a) CK_ASSERT_EQ(a, false)

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
  CK_ASSERT_EQ((alignment & (alignment - 1)), 0);
  return (val & (alignment - 1)) == 0;
}

template <typename T>
requires std::is_integral_v<T>
constexpr T AlignDown(T val, T alignment) {
  CK_ASSERT_EQ((alignment & (alignment - 1)), 0);
  return val & ~(alignment - 1);
}

template <typename T>
requires std::is_integral_v<T>
constexpr T AlignUp(T val, T alignment) {
  CK_ASSERT_EQ((alignment & (alignment - 1)), 0);
  return (val + alignment - 1) & ~(alignment - 1);
}

template <typename T>
requires std::is_integral_v<T>
constexpr T CeilDiv(T val, T quotient) {
  CK_ASSERT_GT(quotient, 0);
  return (val + quotient - 1) / quotient;
}

}  // namespace ckmalloc
