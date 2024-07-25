#pragma once

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

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
  if (!(status).ok())                                   \
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

// Executes an expression that returns an absl::StatusOr<T>, and assigns the
// contained variable to lhs if the error code is OK. If the Status is non-OK,
// generates a test failure and returns from the current function, which must
// have a void return type.
//
// Example: Assigning to an existing value
//   ValueType value;
//   ASSERT_OK_AND_ASSIGN(value, MaybeGetValue(arg));
//
// The value assignment example might expand into:
//   StatusOr<ValueType> status_or_value = MaybeGetValue(arg);
//   ASSERT_OK(status_or_value.status());
//   value = *status_or_value;
#define ASSERT_OK_AND_ASSIGN(lhs, rexpr)                                       \
  do {                                                                         \
    auto _statusor_to_verify = rexpr;                                          \
    if (!_statusor_to_verify.ok()) {                                           \
      FAIL() << #rexpr << " returned error: " << _statusor_to_verify.status(); \
    }                                                                          \
    lhs = *std::move(_statusor_to_verify);                                     \
  } while (false)

#define ASSERT_OK_AND_DEFINE_IMPL(type, lhs, tmp, rexpr)       \
  absl::StatusOr<type> tmp = (rexpr);                          \
  if (!(tmp).ok()) {                                           \
    FAIL() << #rexpr << " returned error: " << (tmp).status(); \
  }                                                            \
  type &lhs = (tmp).value();

// Executes an expression that returns an absl::StatusOr<T>, and defines a new
// variable with given type and name to the result if the error code is OK. If
// the Status is non-OK, generates a test failure and returns from the current
// function, which must have a void return type.
#define ASSERT_OK_AND_DEFINE(type, lhs, rexpr)                            \
  ASSERT_OK_AND_DEFINE_IMPL(                                              \
      type, lhs, UTILS_CONCAT_NAME(__assert_ok_and_define_, __COUNTER__), \
      rexpr)

namespace bench {

namespace internal {

// Implements a gMock matcher that checks that an absl::StaturOr<T> has an OK
// status and that the contained T value matches another matcher.
template <typename StatusOrT>
class IsOkAndHoldsMatcher
    : public ::testing::MatcherInterface<const StatusOrT &> {
  using ValueType = typename StatusOrT::value_type;

 public:
  template <typename MatcherT>
  explicit IsOkAndHoldsMatcher(MatcherT &&value_matcher)
      : value_matcher_(
            ::testing::SafeMatcherCast<const ValueType &>(value_matcher)) {}

  // From testing::MatcherInterface.
  void DescribeTo(std::ostream *os) const override {
    *os << "is OK and contains a value that ";
    value_matcher_.DescribeTo(os);
  }

  // From testing::MatcherInterface.
  void DescribeNegationTo(std::ostream *os) const override {
    *os << "is not OK or contains a value that ";
    value_matcher_.DescribeNegationTo(os);
  }

  // From testing::MatcherInterface.
  bool MatchAndExplain(
      const StatusOrT &status_or,
      ::testing::MatchResultListener *listener) const override {
    if (!status_or.ok()) {
      *listener << "which is not OK";
      return false;
    }

    ::testing::StringMatchResultListener value_listener;
    bool is_a_match =
        value_matcher_.MatchAndExplain(*status_or, &value_listener);
    std::string value_explanation = value_listener.str();
    if (!value_explanation.empty()) {
      *listener << absl::StrCat("which contains a value ", value_explanation);
    }

    return is_a_match;
  }

 private:
  const ::testing::Matcher<const ValueType &> value_matcher_;
};

// A polymorphic IsOkAndHolds() matcher.
//
// IsOkAndHolds() returns a matcher that can be used to process an IsOkAndHolds
// expectation. However, the value type T is not provided when IsOkAndHolds()
// is invoked. The value type is only inferable when the gtest framework
// invokes the matcher with a value. Consequently, the IsOkAndHolds() function
// must return an object that is implicitly convertible to a matcher for
// StatusOr<T>.  gtest refers to such an object as a polymorphic matcher, since
// it can be used to match with more than one type of value.
template <typename ValueMatcherT>
class IsOkAndHoldsGenerator {
 public:
  explicit IsOkAndHoldsGenerator(ValueMatcherT value_matcher)
      : value_matcher_(std::move(value_matcher)) {}

  template <typename T>
  operator ::testing::Matcher<const absl::StatusOr<T> &>() const {
    return ::testing::MakeMatcher(
        new IsOkAndHoldsMatcher<absl::StatusOr<T>>(value_matcher_));
  }

 private:
  const ValueMatcherT value_matcher_;
};

// Implements a gMock matcher that checks whether a status container (e.g.
// absl::Status or absl::StatusOr<T>) has an OK status.
class IsOkMatcher {
 public:
  IsOkMatcher() = default;

  // Describes the OK expectation.
  void DescribeTo(std::ostream *os) const {
    *os << "is OK";
  }

  // Describes the negative OK expectation.
  void DescribeNegationTo(std::ostream *os) const {
    *os << "is not OK";
  }

  // Tests whether |status_container|'s OK value meets this matcher's
  // expectation.
  template <class T>
  bool MatchAndExplain(const T &status_container,
                       ::testing::MatchResultListener *listener) const {
    if (!status_container.ok()) {
      *listener << "which is not OK";
      return false;
    }
    return true;
  }
};

}  // namespace internal

// Returns a gMock matcher that expects an absl::StatusOr<T> object to have an
// OK status and for the contained T object to match |value_matcher|.
//
// Example:
//
//     StatusOr<string> raven_speech_result = raven.Speak();
//     EXPECT_THAT(raven_speech_result, IsOkAndHolds(HasSubstr("nevermore")));
//
// If foo is an object of type T and foo_result is an object of type
// StatusOr<T>, you can write:
//
//     EXPECT_THAT(foo_result, IsOkAndHolds(foo));
//
// instead of:
//
//     EXPECT_THAT(foo_result, IsOkAndHolds(Eq(foo)));
template <typename ValueMatcherT>
internal::IsOkAndHoldsGenerator<ValueMatcherT> IsOkAndHolds(
    ValueMatcherT value_matcher) {
  return internal::IsOkAndHoldsGenerator<ValueMatcherT>(value_matcher);
}

// Returns an internal::IsOkMatcherGenerator, which may be typecast to a
// Matcher<absl::Status> or Matcher<absl::StatusOr<T>>. These gMock matchers
// test that a given status container has an OK status.
inline ::testing::PolymorphicMatcher<internal::IsOkMatcher> IsOk() {
  return ::testing::MakePolymorphicMatcher(internal::IsOkMatcher());
}

}  // namespace bench
