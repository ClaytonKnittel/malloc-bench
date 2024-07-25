#include "gtest/gtest.h"

#include "src/correctness_checker.h"
#include "src/util.h"

namespace bench {

TEST(TestCorrectness, Simple) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple.trace"), IsOk());
}

TEST(TestCorrectness, SimpleRealloc) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple_realloc.trace"),
              IsOk());
}

TEST(TestCorrectness, Onoro) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/onoro.trace"), IsOk());
}

TEST(TestCorrectness, OnoroCC) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/onoro-cc.trace"),
              IsOk());
}

}  // namespace bench
