#include "gtest/gtest.h"
<<<<<<< HEAD
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/correctness_checker.h"
#include "src/mmap_heap_factory.h"
#include "src/tracefile_reader.h"

namespace bench {

class TestCorrectness : public ::testing::Test {
 public:
  static absl::Status Check(const std::string& tracefile) {
    DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(tracefile));
    MMapHeapFactory heap_factory;
    return bench::CorrectnessChecker::Check(reader, heap_factory);
  }
};

TEST_F(TestCorrectness, All) {
  ASSERT_THAT(Check("traces/bdd-aa32.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/bdd-aa4.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/bdd-ma4.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/bdd-nq7.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/cbit-abs.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/cbit-parity.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/cbit-satadd.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/cbit-xyz.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/ngram-fox1.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/ngram-gulliver1.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/ngram-gulliver2.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/ngram-moby1.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/ngram-shake1.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-array.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-array-short.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-mix.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-mix-realloc.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-mix-short.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-string.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-string-short.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-struct.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/syn-struct-short.trace"), util::IsOk());
  ASSERT_THAT(Check("traces/test.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Server) {
  ASSERT_THAT(Check("traces/server.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Simple) {
  ASSERT_THAT(Check("traces/simple.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SimpleCalloc) {
  ASSERT_THAT(Check("traces/simple_calloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SimpleRealloc) {
  ASSERT_THAT(Check("traces/simple_realloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Onoro) {
  ASSERT_THAT(Check("traces/onoro.trace"), util::IsOk());
}

TEST_F(TestCorrectness, OnoroCC) {
  ASSERT_THAT(Check("traces/onoro-cc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Zero) {
  ASSERT_THAT(Check("traces/test-zero.trace"), util::IsOk());
=======
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
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
}

TEST(TestCorrectness, Zero) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/test-zero.trace"),
<<<<<<< HEAD
              IsOk());
=======
              util::IsOk());
>>>>>>> d3b973fd6e938786ae4ec0560b204de2d3ba8e58
}

}  // namespace bench
