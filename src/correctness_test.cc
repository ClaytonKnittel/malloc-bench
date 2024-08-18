#include <cstdlib>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/state_test_fixture.h"
#include "src/correctness_checker.h"

namespace bench {

class TestCkMalloc {
 public:
  using TestState = ckmalloc::StateFixture::TestState;

  static ckmalloc::StateFixture* fixture_;

  static void initialize_heap() {
    fixture_ = new ckmalloc::StateFixture();
  }

  static void destroy_heap() {
    delete fixture_;
    fixture_ = nullptr;
  }

  static void* malloc(size_t size) {
    if (size == 0) {
      return nullptr;
    }
    void* result = fixture_->MainAllocator().Alloc(size);
    absl::Status valid = fixture_->ValidateHeap();
    if (!valid.ok()) {
      std::cerr << valid << std::endl;
      std::abort();
    }

    return result;
  }

  static void* calloc(size_t nmemb, size_t size) {
    void* block = malloc(nmemb * size);
    if (block != nullptr) {
      memset(block, 0, nmemb * size);
    }
    return block;
  }

  static void* realloc(void* ptr, size_t size) {
    CK_ASSERT_NE(size, 0);
    if (ptr == nullptr) {
      return malloc(size);
    }
    void* result = fixture_->MainAllocator().Realloc(ptr, size);
    absl::Status valid = fixture_->ValidateHeap();
    if (!valid.ok()) {
      std::cerr << valid << std::endl;
      std::abort();
    }

    return result;
  }

  static void free(void* ptr) {
    if (ptr == nullptr) {
      return;
    }
    fixture_->MainAllocator().Free(ptr);

    absl::Status valid = fixture_->ValidateHeap();
    if (!valid.ok()) {
      std::cerr << valid << std::endl;
      std::abort();
    }
  }
};

ckmalloc::StateFixture* TestCkMalloc::fixture_ = nullptr;

class TestCorrectness : public ::testing::Test {
 public:
  using Checker = bench::CorrectnessCheckerImpl<TestCkMalloc>;

  TestCorrectness() {
    TestCkMalloc::initialize_heap();
  }

  ~TestCorrectness() override {
    TestCkMalloc::destroy_heap();
  }
};

TEST_F(TestCorrectness, BddAa32) {
  ASSERT_THAT(Checker::Check("traces/bdd-aa32.trace"), util::IsOk());
}

TEST_F(TestCorrectness, BddAa4) {
  ASSERT_THAT(Checker::Check("traces/bdd-aa4.trace"), util::IsOk());
}

TEST_F(TestCorrectness, bddma4) {
  ASSERT_THAT(Checker::Check("traces/bdd-ma4.trace"), util::IsOk());
}

TEST_F(TestCorrectness, bddnq7) {
  ASSERT_THAT(Checker::Check("traces/bdd-nq7.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitabs) {
  ASSERT_THAT(Checker::Check("traces/cbit-abs.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitparity) {
  ASSERT_THAT(Checker::Check("traces/cbit-parity.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitsatadd) {
  ASSERT_THAT(Checker::Check("traces/cbit-satadd.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitxyz) {
  ASSERT_THAT(Checker::Check("traces/cbit-xyz.trace"), util::IsOk());
}

TEST_F(TestCorrectness, ngramfox1) {
  ASSERT_THAT(Checker::Check("traces/ngram-fox1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, ngramgulliver1) {
  ASSERT_THAT(Checker::Check("traces/ngram-gulliver1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, ngramgulliver2) {
  ASSERT_THAT(Checker::Check("traces/ngram-gulliver2.trace"), util::IsOk());
}

TEST_F(TestCorrectness, ngrammoby1) {
  ASSERT_THAT(Checker::Check("traces/ngram-moby1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, ngramshake1) {
  ASSERT_THAT(Checker::Check("traces/ngram-shake1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synarray) {
  ASSERT_THAT(Checker::Check("traces/syn-array.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synarrayshort) {
  ASSERT_THAT(Checker::Check("traces/syn-array-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synmix) {
  ASSERT_THAT(Checker::Check("traces/syn-mix.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synmixrealloc) {
  ASSERT_THAT(Checker::Check("traces/syn-mix-realloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synmixshort) {
  ASSERT_THAT(Checker::Check("traces/syn-mix-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synstring) {
  ASSERT_THAT(Checker::Check("traces/syn-string.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synstringshort) {
  ASSERT_THAT(Checker::Check("traces/syn-string-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, synstruct) {
  ASSERT_THAT(Checker::Check("traces/syn-struct.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynStructShort) {
  ASSERT_THAT(Checker::Check("traces/syn-struct-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Test) {
  ASSERT_THAT(Checker::Check("traces/test.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Server) {
  ASSERT_THAT(Checker::Check("traces/server.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Simple) {
  ASSERT_THAT(Checker::Check("traces/simple.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SimpleCalloc) {
  ASSERT_THAT(Checker::Check("traces/simple_calloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SimpleRealloc) {
  ASSERT_THAT(Checker::Check("traces/simple_realloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Onoro) {
  ASSERT_THAT(Checker::Check("traces/onoro.trace"), util::IsOk());
}

TEST_F(TestCorrectness, OnoroCC) {
  ASSERT_THAT(Checker::Check("traces/onoro-cc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Zero) {
  ASSERT_THAT(Checker::Check("traces/test-zero.trace"), util::IsOk());
}

}  // namespace bench
