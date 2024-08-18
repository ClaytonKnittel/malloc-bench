#include <cstdlib>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/main_allocator_test_fixture.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/correctness_checker.h"

namespace bench {

class TestCkMalloc {
  friend class TestCorrectness;

 public:
  static void initialize_heap();
  static void* malloc(size_t size);
  static void* calloc(size_t nmemb, size_t size);
  static void* realloc(void* ptr, size_t size);
  static void free(void* ptr);

 private:
  static class TestCorrectness* fixture_;
};

TestCorrectness* TestCkMalloc::fixture_ = nullptr;

class TestCorrectness : public ::testing::Test {
 public:
  using Checker = bench::CorrectnessCheckerImpl<TestCkMalloc>;

  static constexpr size_t kNumPages = 64;

  TestCorrectness()
      : heap_(std::make_shared<ckmalloc::TestHeap>(kNumPages)),
        slab_map_(std::make_shared<ckmalloc::TestSlabMap>()),
        slab_manager_fixture_(
            std::make_shared<ckmalloc::SlabManagerFixture>(heap_, slab_map_)),
        metadata_manager_fixture_(
            std::make_shared<ckmalloc::MetadataManagerFixture>(
                heap_, slab_map_, slab_manager_fixture_)),
        small_allocator_fixture_(
            std::make_shared<ckmalloc::SmallAllocatorFixture>(
                heap_, slab_map_, slab_manager_fixture_)),
        main_allocator_fixture_(
            std::make_shared<ckmalloc::MainAllocatorFixture>(
                heap_, slab_map_, slab_manager_fixture_)) {
    TestCkMalloc::fixture_ = this;
  }

  ~TestCorrectness() override {
    TestCkMalloc::fixture_ = nullptr;
  }

  ckmalloc::MainAllocatorFixture::TestMainAllocator& MainAllocator() {
    return main_allocator_fixture_->MainAllocator();
  }

  absl::Status RunTrace(const std::string& trace) {
    return Checker::Check(trace, heap_.get());
  }

  absl::Status ValidateHeap() const;

 private:
  std::shared_ptr<ckmalloc::TestHeap> heap_;
  std::shared_ptr<ckmalloc::TestSlabMap> slab_map_;
  std::shared_ptr<ckmalloc::SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<ckmalloc::MetadataManagerFixture> metadata_manager_fixture_;
  std::shared_ptr<ckmalloc::SmallAllocatorFixture> small_allocator_fixture_;
  std::shared_ptr<ckmalloc::MainAllocatorFixture> main_allocator_fixture_;
};

/* static */
void TestCkMalloc::initialize_heap() {}

/* static */
void* TestCkMalloc::malloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }
  std::cout << "malloc(" << size << ")" << std::endl;
  void* result = fixture_->MainAllocator().Alloc(size);
  absl::Status valid = fixture_->ValidateHeap();
  if (!valid.ok()) {
    std::cerr << valid << std::endl;
    std::abort();
  }

  return result;
}

/* static */
void* TestCkMalloc::calloc(size_t nmemb, size_t size) {
  void* block = malloc(nmemb * size);
  if (block != nullptr) {
    memset(block, 0, nmemb * size);
  }
  return block;
}

/* static */
void* TestCkMalloc::realloc(void* ptr, size_t size) {
  CK_ASSERT_NE(size, 0);
  if (ptr == nullptr) {
    return malloc(size);
  }
  std::cout << "realloc(" << ptr << ", " << size << ")" << std::endl;
  void* result = fixture_->MainAllocator().Realloc(ptr, size);
  absl::Status valid = fixture_->ValidateHeap();
  if (!valid.ok()) {
    std::cerr << valid << std::endl;
    std::abort();
  }

  return result;
}

/* static */
void TestCkMalloc::free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  std::cout << "free(" << ptr << ")" << std::endl;
  fixture_->MainAllocator().Free(ptr);

  absl::Status valid = fixture_->ValidateHeap();
  if (!valid.ok()) {
    std::cerr << valid << std::endl;
    std::abort();
  }
}

absl::Status TestCorrectness::ValidateHeap() const {
  RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
  RETURN_IF_ERROR(metadata_manager_fixture_->ValidateHeap());
  RETURN_IF_ERROR(small_allocator_fixture_->ValidateHeap());
  RETURN_IF_ERROR(main_allocator_fixture_->ValidateHeap());
  return absl::OkStatus();
}

// TEST_F(TestCorrectness, bddaa32) {
//   ASSERT_THAT(RunTrace("traces/bdd-aa32.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, bddaa4) {
//   ASSERT_THAT(RunTrace("traces/bdd-aa4.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, bddma4) {
//   ASSERT_THAT(RunTrace("traces/bdd-ma4.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, bddnq7) {
//   ASSERT_THAT(RunTrace("traces/bdd-nq7.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, cbitabs) {
//   ASSERT_THAT(RunTrace("traces/cbit-abs.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, cbitparity) {
//   ASSERT_THAT(RunTrace("traces/cbit-parity.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, cbitsatadd) {
//   ASSERT_THAT(RunTrace("traces/cbit-satadd.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, cbitxyz) {
//   ASSERT_THAT(RunTrace("traces/cbit-xyz.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, NgramFox1) {
//   ASSERT_THAT(RunTrace("traces/ngram-fox1.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, NgramGulliver1) {
//   ASSERT_THAT(RunTrace("traces/ngram-gulliver1.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, NgramGulliver2) {
//   ASSERT_THAT(RunTrace("traces/ngram-gulliver2.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, NgramMoby1) {
//   ASSERT_THAT(RunTrace("traces/ngram-moby1.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, NgramShake1) {
//   ASSERT_THAT(RunTrace("traces/ngram-shake1.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynArray) {
//   ASSERT_THAT(RunTrace("traces/syn-array.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynArrayShort) {
//   ASSERT_THAT(RunTrace("traces/syn-array-short.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynMix) {
//   ASSERT_THAT(RunTrace("traces/syn-mix.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynMixRealloc) {
//   ASSERT_THAT(RunTrace("traces/syn-mix-realloc.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynMixShort) {
//   ASSERT_THAT(RunTrace("traces/syn-mix-short.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynString) {
//   ASSERT_THAT(RunTrace("traces/syn-string.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynStringShort) {
//   ASSERT_THAT(RunTrace("traces/syn-string-short.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SynStruct) {
//   ASSERT_THAT(RunTrace("traces/syn-struct.trace"), util::IsOk());
// }

TEST_F(TestCorrectness, SynStructShort) {
  ASSERT_THAT(RunTrace("traces/syn-struct-short.trace"), util::IsOk());
}

// TEST_F(TestCorrectness, Test) {
//   ASSERT_THAT(RunTrace("traces/test.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, Server) {
//   ASSERT_THAT(RunTrace("traces/server.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, Simple) {
//   ASSERT_THAT(RunTrace("traces/simple.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SimpleCalloc) {
//   ASSERT_THAT(RunTrace("traces/simple_calloc.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, SimpleRealloc) {
//   ASSERT_THAT(RunTrace("traces/simple_realloc.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, Onoro) {
//   ASSERT_THAT(RunTrace("traces/onoro.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, OnoroCC) {
//   ASSERT_THAT(RunTrace("traces/onoro-cc.trace"), util::IsOk());
// }
//
// TEST_F(TestCorrectness, Zero) {
//   ASSERT_THAT(RunTrace("traces/test-zero.trace"), util::IsOk());
// }

}  // namespace bench
