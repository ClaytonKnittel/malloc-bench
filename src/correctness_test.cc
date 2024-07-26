#include "gtest/gtest.h"

#include "src/correctness_checker.h"
#include "src/util.h"

namespace bench {

TEST(TestCorrectness, All) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-aa32.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-aa4.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-ma4.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-nq7.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-abs.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-parity.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-satadd.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-xyz.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-fox1.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-gulliver1.trace"),
              IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-gulliver2.trace"),
              IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-moby1.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-shake1.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-array.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-array-short.trace"),
              IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-mix.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-mix-realloc.trace"),
              IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-mix-short.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-string.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-string-short.trace"),
              IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-struct.trace"), IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-struct-short.trace"),
              IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("test.trace"), IsOk());
}

TEST(TestCorrectness, Server) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/server.trace"), IsOk());
}

TEST(TestCorrectness, Simple) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple.trace"), IsOk());
}

TEST(TestCorrectness, SimpleCalloc) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple_calloc.trace"),
              IsOk());
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

TEST(TestCorrectness, Zero) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/test-zero.trace"),
              IsOk());
}

}  // namespace bench
