#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <type_traits>

#ifdef NDEBUG

#define CK_ASSERT_MSG(cond, message)              \
  _Pragma("GCC diagnostic push");                 \
  _Pragma("GCC diagnostic ignored \"-Wassume\""); \
  __builtin_assume(cond);                         \
  _Pragma("GCC diagnostic pop")

#define CK_UNREACHABLE(msg) __builtin_unreachable()

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

#define CK_UNREACHABLE(msg) \
  CK_ASSERT_MSG(false, "reached unreachable code: " msg)

#endif

// clang-format off
#define CK_ASSERT_INFIX(a, b, op, neg) \
  CK_ASSERT_MSG((a) op (b), (a) << (" " #neg " ") << (b))
// clang-format on

#define CK_ASSERT_EQ(a, b) CK_ASSERT_INFIX(a, b, ==, !=)
#define CK_ASSERT_NE(a, b) CK_ASSERT_INFIX(a, b, !=, ==)
#define CK_ASSERT_LT(a, b) CK_ASSERT_INFIX(a, b, <, >=)
#define CK_ASSERT_LE(a, b) CK_ASSERT_INFIX(a, b, <=, >)
#define CK_ASSERT_GT(a, b) CK_ASSERT_INFIX(a, b, >, <=)
#define CK_ASSERT_GE(a, b) CK_ASSERT_INFIX(a, b, >=, <)
#define CK_ASSERT_TRUE(a)  CK_ASSERT_EQ(a, true)
#define CK_ASSERT_FALSE(a) CK_ASSERT_EQ(a, false)

#define CK_EXPECT_TRUE(cond)  __builtin_expect((long) (cond), (long) 1)
#define CK_EXPECT_FALSE(cond) __builtin_expect((long) (cond), (long) 0)

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

// Returns a - b by integer value.
template <typename T, typename U>
constexpr size_t PtrDistance(const T* a, const U* b) {
  return static_cast<size_t>(reinterpret_cast<const uint8_t*>(a) -
                             reinterpret_cast<const uint8_t*>(b));
}

// Returns ptr + int, returning void*
template <typename U = void, typename T, typename Int>
requires std::is_integral_v<Int>
constexpr U* PtrAdd(T* a, Int offset) {
  using PtrT = std::conditional_t<std::is_const_v<T>, const uint8_t, uint8_t>;
  U* result = reinterpret_cast<U*>(reinterpret_cast<PtrT*>(a) + offset);
  if constexpr (!std::is_void_v<U>) {
    CK_ASSERT_TRUE(IsAligned(reinterpret_cast<uintptr_t>(result), alignof(U)));
  }
  return result;
}

// Returns ptr - int, returning void*
template <typename U = void, typename T, typename Int>
requires std::is_integral_v<Int>
constexpr U* PtrSub(T* a, Int offset) {
  using PtrT = std::conditional_t<std::is_const_v<T>, const uint8_t, uint8_t>;
  U* result = reinterpret_cast<U*>(reinterpret_cast<PtrT*>(a) - offset);
  if constexpr (!std::is_void_v<U>) {
    CK_ASSERT_TRUE(IsAligned(reinterpret_cast<uintptr_t>(result), alignof(U)));
  }
  return result;
}

class AlignedAlloc {
 public:
  explicit AlignedAlloc(size_t size, size_t alignment) {
    memory_region_ = new uint8_t[size + alignment];
    uintptr_t start = reinterpret_cast<uintptr_t>(memory_region_);
    uintptr_t alignment_offset = AlignUp(start, alignment) - start;
    start_ = memory_region_ + alignment_offset;
    CK_ASSERT_TRUE(IsAligned(reinterpret_cast<uintptr_t>(start_), alignment));
  }

  ~AlignedAlloc() {
    delete[] memory_region_;
  }

  void* RegionStart() {
    return start_;
  }

  const void* RegionStart() const {
    return start_;
  }

 private:
  // Where the allocated memory starts
  uint8_t* memory_region_;
  // Where the aligned region starts. We allocate extra memory so the start of
  // the region will be aligned.
  uint8_t* start_;
};

}  // namespace ckmalloc
