#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/correctness_checker.h"

namespace bench {

TEST(TestCorrectness, All) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-aa32.trace"), util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-aa4.trace"), util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-ma4.trace"), util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-nq7.trace"), util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-abs.trace"), util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-parity.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-satadd.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-xyz.trace"), util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-fox1.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-gulliver1.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-gulliver2.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-moby1.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("ngram-shake1.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-array.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-array-short.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-mix.trace"), util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-mix-realloc.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-mix-short.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-string.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-string-short.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-struct.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-struct-short.trace"),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("test.trace"), util::IsOk());
}

TEST(TestCorrectness, Server) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/server.trace"),
              util::IsOk());
}

TEST(TestCorrectness, Simple) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple.trace"),
              util::IsOk());
}

TEST(TestCorrectness, SimpleCalloc) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple_calloc.trace"),
              util::IsOk());
}

TEST(TestCorrectness, SimpleRealloc) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple_realloc.trace"),
              util::IsOk());
}

TEST(TestCorrectness, Onoro) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/onoro.trace"),
              util::IsOk());
}

TEST(TestCorrectness, OnoroCC) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/onoro-cc.trace"),
              util::IsOk());
}

TEST(TestCorrectness, Zero) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/test-zero.trace"),
              util::IsOk());
}

}  // namespace bench
