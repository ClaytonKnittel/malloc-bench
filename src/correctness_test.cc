#include "gtest/gtest.h"
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
}

TEST(TestCorrectness, Zero) {
  ASSERT_THAT(bench::CorrectnessChecker::Check("traces/test-zero.trace"),
              IsOk());
}

}  // namespace bench
