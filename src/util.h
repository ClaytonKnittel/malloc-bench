#pragma once

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#define RETURN_IF_ERROR(expr)      \
  do {                             \
    absl::Status _status = (expr); \
    if (!_status.ok())             \
      return _status;              \
  } while (0)

// Internal helper for concatenating macro values.
#define UTILS_CONCAT_NAME_INNER(x, y) x##y
#define UTILS_CONCAT_NAME(x, y)       UTILS_CONCAT_NAME_INNER(x, y)

template <typename T>
absl::Status DoAssignOrReturn(T &lhs, absl::StatusOr<T> result) {
  if (result.ok()) {
    lhs = result.value();
  }
  return result.status();
}

#define ASSIGN_OR_RETURN_IMPL(status, lhs, rexpr)       \
  absl::Status status = DoAssignOrReturn(lhs, (rexpr)); \
  if (!status.ok())                                     \
    return status;

// Executes an expression that returns a util::StatusOr, extracting its value
// into the variable defined by lhs (or returning on error).
//
// Example: Assigning to an existing value
//   ValueType value;
//   ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
//
// WARNING: ASSIGN_OR_RETURN expands into multiple statements; it cannot be used
//  in a single statement (e.g. as the body of an if statement without {})!
#define ASSIGN_OR_RETURN(lhs, rexpr)                                           \
  ASSIGN_OR_RETURN_IMPL(UTILS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, \
                        rexpr);

#define DEFINE_OR_RETURN_IMPL(type, lhs, tmp, rexpr) \
  absl::StatusOr<type> tmp = (rexpr);                \
  if (!(tmp).ok()) {                                 \
    return (tmp).status();                           \
  }                                                  \
  type &lhs = (tmp).value();

// Executes an expression that returns an absl::StatusOr<T>, and defines a new
// variable with given type and name to the result if the error code is OK. If
// the Status is non-OK, returns the error.
#define DEFINE_OR_RETURN(type, lhs, rexpr) \
  DEFINE_OR_RETURN_IMPL(                   \
      type, lhs, UTILS_CONCAT_NAME(__define_or_return_, __COUNTER__), rexpr)
