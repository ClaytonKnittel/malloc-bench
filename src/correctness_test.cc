#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/correctness_checker.h"
#include "src/mmap_heap_factory.h"

namespace bench {

TEST(TestCorrectness, All) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-aa32.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-aa4.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-ma4.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("bdd-nq7.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-abs.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("cbit-parity.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("cbit-satadd.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("cbit-xyz.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("ngram-fox1.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("ngram-gulliver1.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("ngram-gulliver2.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("ngram-moby1.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("ngram-shake1.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-array.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("syn-array-short.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("syn-mix.trace", heap_factory),
              util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("syn-mix-realloc.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("syn-mix-short.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("syn-string.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("syn-string-short.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("syn-struct.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("syn-struct-short.trace", heap_factory),
      util::IsOk());
  ASSERT_THAT(bench::CorrectnessChecker::Check("test.trace", heap_factory),
              util::IsOk());
}

TEST(TestCorrectness, Server) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("traces/server.trace", heap_factory),
      util::IsOk());
}

TEST(TestCorrectness, Simple) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("traces/simple.trace", heap_factory),
      util::IsOk());
}

TEST(TestCorrectness, SimpleCalloc) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple_calloc.trace",
                                               heap_factory),
              util::IsOk());
}

TEST(TestCorrectness, SimpleRealloc) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/simple_realloc.trace",
                                               heap_factory),
              util::IsOk());
}

TEST(TestCorrectness, Onoro) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("traces/onoro.trace", heap_factory),
      util::IsOk());
}

TEST(TestCorrectness, OnoroCC) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("traces/onoro-cc.trace", heap_factory),
      util::IsOk());
}

TEST(TestCorrectness, Zero) {
  MMapHeapFactory heap_factory;
  ASSERT_THAT(
      bench::CorrectnessChecker::Check("traces/test-zero.trace", heap_factory),
      util::IsOk());
}

}  // namespace bench
