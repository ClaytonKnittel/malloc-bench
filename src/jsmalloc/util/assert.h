#pragma once

#include <exception>
#include <iostream>

#include "absl/strings/str_cat.h"

#ifndef NDEBUG

#define DCHECK(cond, msg)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::string message = msg;                                               \
      std::cerr << "`" << #cond << "` assertion failed at " << __FILE__ << ":" \
                << __LINE__;                                                   \
      if (message != "") {                                                     \
        std::cerr << " (" << message << ")" << std::endl;                      \
      } else {                                                                 \
        std::cerr << std::endl;                                                \
      }                                                                        \
      std::terminate();                                                        \
    }                                                                          \
  } while (false)

#else

#define DCHECK(cond, msg) \
  do {                    \
  } while (false)

#endif

#define DCHECK_INFIX(a, b, op, neg) \
  DCHECK(a op b, absl::StrCat(a, " ", #neg, " ", b))

#define DCHECK_EQ(a, b)    DCHECK_INFIX(a, b, ==, !=)
#define DCHECK_NE(a, b)    DCHECK_INFIX(a, b, !=, ==)
#define DCHECK_GT(a, b)    DCHECK_INFIX(a, b, >, <=)
#define DCHECK_GE(a, b)    DCHECK_INFIX(a, b, >=, <)
#define DCHECK_LT(a, b)    DCHECK_INFIX(a, b, <, >=)
#define DCHECK_LE(a, b)    DCHECK_INFIX(a, b, <=, >)
#define DCHECK_TRUE(expr)  DCHECK_EQ(expr, true)
#define DCHECK_FALSE(expr) DCHECK_EQ(expr, false)

namespace assert {
namespace internal {

template <typename T>
T* DieIfNull(T* ptr, const char* expr, const char* file, int line) {
#ifndef NDEBUG
  if (ptr == nullptr) {
    std::cerr << "`" << expr << "` was unexpectedly null at " << file << ":"
              << line << "\n";
    std::terminate();
  }
#endif
  return ptr;
}
}  // namespace internal
}  // namespace assert

#define DCHECK_NON_NULL(ptr) \
  assert::internal::DieIfNull(ptr, #ptr, __FILE__, __LINE__)
