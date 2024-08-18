#include <cstdlib>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/main_allocator_test_fixture.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

// #define PRINT

class TestCkMalloc : public TracefileExecutor {
  friend class TestCorrectness;

 public:
  explicit TestCkMalloc(TracefileReader&& tracefile_reader,
                        class TestCorrectness* fixture)
      : TracefileExecutor(std::move(tracefile_reader)), fixture_(fixture) {}

  void InitializeHeap() override;
  absl::StatusOr<void*> Malloc(size_t size) override;
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override;
  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override;
  absl::Status Free(void* ptr) override;

 private:
  class TestCorrectness* fixture_;
};

class TestCorrectness : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = (1 << 17);

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
                heap_, slab_map_, slab_manager_fixture_,
                small_allocator_fixture_)) {}

  ckmalloc::MainAllocatorFixture::TestMainAllocator& MainAllocator() {
    return main_allocator_fixture_->MainAllocator();
  }

  absl::Status RunTrace(const std::string& trace) {
    DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(trace));
    TestCkMalloc test(std::move(reader), this);
    return test.Run();
  }

  absl::Status ValidateHeap() const {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(metadata_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateHeap());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<ckmalloc::TestHeap> heap_;
  std::shared_ptr<ckmalloc::TestSlabMap> slab_map_;
  std::shared_ptr<ckmalloc::SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<ckmalloc::MetadataManagerFixture> metadata_manager_fixture_;
  std::shared_ptr<ckmalloc::SmallAllocatorFixture> small_allocator_fixture_;
  std::shared_ptr<ckmalloc::MainAllocatorFixture> main_allocator_fixture_;
};

void TestCkMalloc::InitializeHeap() {}

absl::StatusOr<void*> TestCkMalloc::Malloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }
#ifdef PRINT
  std::cout << "malloc(" << size << ")" << std::endl;
#endif
  void* result = fixture_->MainAllocator().Alloc(size);
#ifdef PRINT
  std::cout << "returned " << result << std::endl;
#endif
  if (result == nullptr) {
    return absl::FailedPreconditionError("Returned nullptr from malloc");
  }

  RETURN_IF_ERROR(fixture_->ValidateHeap());
  return result;
}

absl::StatusOr<void*> TestCkMalloc::Calloc(size_t nmemb, size_t size) {
  return Malloc(nmemb * size);
}

absl::StatusOr<void*> TestCkMalloc::Realloc(void* ptr, size_t size) {
  if (ptr == nullptr) {
    return Malloc(size);
  }

  CK_ASSERT_NE(size, 0);
#ifdef PRINT
  std::cout << "realloc(" << ptr << ", " << size << ")" << std::endl;
#endif
  void* result = fixture_->MainAllocator().Realloc(ptr, size);
#ifdef PRINT
  std::cout << "returned " << result << std::endl;
#endif
  if (result == nullptr) {
    return absl::FailedPreconditionError("Returned nullptr from realloc");
  }

  RETURN_IF_ERROR(fixture_->ValidateHeap());
  return result;
}

absl::Status TestCkMalloc::Free(void* ptr) {
  if (ptr == nullptr) {
    return absl::OkStatus();
  }

#ifdef PRINT
  std::cout << "free(" << ptr << ")" << std::endl;
#endif
  fixture_->MainAllocator().Free(ptr);

  return fixture_->ValidateHeap();
}

TEST_F(TestCorrectness, bddaa32) {
  ASSERT_THAT(RunTrace("traces/bdd-aa32.trace"), util::IsOk());
}

TEST_F(TestCorrectness, bddaa4) {
  ASSERT_THAT(RunTrace("traces/bdd-aa4.trace"), util::IsOk());
}

TEST_F(TestCorrectness, bddma4) {
  ASSERT_THAT(RunTrace("traces/bdd-ma4.trace"), util::IsOk());
}

TEST_F(TestCorrectness, bddnq7) {
  ASSERT_THAT(RunTrace("traces/bdd-nq7.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitabs) {
  ASSERT_THAT(RunTrace("traces/cbit-abs.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitparity) {
  ASSERT_THAT(RunTrace("traces/cbit-parity.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitsatadd) {
  ASSERT_THAT(RunTrace("traces/cbit-satadd.trace"), util::IsOk());
}

TEST_F(TestCorrectness, cbitxyz) {
  ASSERT_THAT(RunTrace("traces/cbit-xyz.trace"), util::IsOk());
}

TEST_F(TestCorrectness, NgramFox1) {
  ASSERT_THAT(RunTrace("traces/ngram-fox1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, NgramGulliver1) {
  ASSERT_THAT(RunTrace("traces/ngram-gulliver1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, NgramGulliver2) {
  ASSERT_THAT(RunTrace("traces/ngram-gulliver2.trace"), util::IsOk());
}

TEST_F(TestCorrectness, NgramMoby1) {
  ASSERT_THAT(RunTrace("traces/ngram-moby1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, NgramShake1) {
  ASSERT_THAT(RunTrace("traces/ngram-shake1.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynArray) {
  ASSERT_THAT(RunTrace("traces/syn-array.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynArrayShort) {
  ASSERT_THAT(RunTrace("traces/syn-array-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynMix) {
  ASSERT_THAT(RunTrace("traces/syn-mix.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynMixRealloc) {
  ASSERT_THAT(RunTrace("traces/syn-mix-realloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynMixShort) {
  ASSERT_THAT(RunTrace("traces/syn-mix-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynString) {
  ASSERT_THAT(RunTrace("traces/syn-string.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynStringShort) {
  ASSERT_THAT(RunTrace("traces/syn-string-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynStruct) {
  ASSERT_THAT(RunTrace("traces/syn-struct.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SynStructShort) {
  ASSERT_THAT(RunTrace("traces/syn-struct-short.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Test) {
  ASSERT_THAT(RunTrace("traces/test.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Server) {
  ASSERT_THAT(RunTrace("traces/server.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Simple) {
  ASSERT_THAT(RunTrace("traces/simple.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SimpleCalloc) {
  ASSERT_THAT(RunTrace("traces/simple_calloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, SimpleRealloc) {
  ASSERT_THAT(RunTrace("traces/simple_realloc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Onoro) {
  ASSERT_THAT(RunTrace("traces/onoro.trace"), util::IsOk());
}

TEST_F(TestCorrectness, OnoroCC) {
  ASSERT_THAT(RunTrace("traces/onoro-cc.trace"), util::IsOk());
}

TEST_F(TestCorrectness, Zero) {
  ASSERT_THAT(RunTrace("traces/test-zero.trace"), util::IsOk());
}

}  // namespace bench
