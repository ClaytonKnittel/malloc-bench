#pragma once

#include <exception>
#include <iostream>

#include "absl/strings/str_cat.h"

#ifndef NDEBUG

#define DCHECK(cond, msg)                                \
  do {                                                   \
    if (!(cond)) {                                       \
      std::string message = msg;                         \
      std::cerr << "`" << #cond << "` assertion failed"; \
      if (message != "") {                               \
        std::cerr << ": " << message << std::endl;       \
      } else {                                           \
        std::cerr << std::endl;                          \
      }                                                  \
      std::terminate();                                  \
    }                                                    \
  } while (false)

#else

#define DCHECK(cond, msg) \
  do {                    \
  } while (false)

#endif

#define DCHECK_INFIX(a, b, op, neg) \
  DCHECK(a op b, absl::StrCat(a, " ", #neg, " ", b))

#define DCHECK_EQ(a, b) DCHECK_INFIX(a, b, ==, !=)
#define DCHECK_NE(a, b) DCHECK_INFIX(a, b, !=, ==)
#define DCHECK_GT(a, b) DCHECK_INFIX(a, b, >, <=)
#define DCHECK_GE(a, b) DCHECK_INFIX(a, b, >=, <)
#define DCHECK_LT(a, b) DCHECK_INFIX(a, b, <, >=)
#define DCHECK_LE(a, b) DCHECK_INFIX(a, b, <=, >)

namespace internal {
template <typename T>
T* DieIfNull(T* ptr) {
  DCHECK_NE(ptr, nullptr);
  return ptr;
}
}  // namespace internal

#define DCHECK_NON_NULL(a, b) DCHECK_INFIX(a, b, <=, >)
