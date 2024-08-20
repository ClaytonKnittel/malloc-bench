#include <cstdlib>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
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

using util::IsOk;

// #define PRINT

class TestMetadataAlloc : public ckmalloc::TestMetadataAllocInterface {
 public:
  explicit TestMetadataAlloc(class TestCorrectness* fixture)
      : fixture_(fixture) {}

  ckmalloc::Slab* SlabAlloc() override;
  void SlabFree(ckmalloc::MappedSlab* slab) override;
  void* Alloc(size_t size, size_t alignment) override;

  // Test-only function to delete memory allocted by `Alloc`.
  void ClearAllAllocs() override;

 private:
  class TestCorrectness* const fixture_;
};

class TestCkMalloc : public TracefileExecutor {
  friend class TestCorrectness;

 public:
  explicit TestCkMalloc(TracefileReader&& tracefile_reader,
                        class TestCorrectness* fixture,
                        uint32_t validate_every_n)
      : TracefileExecutor(std::move(tracefile_reader)),
        fixture_(fixture),
        allocator_(fixture),
        validate_every_n_(validate_every_n) {}

  void InitializeHeap() override;
  absl::StatusOr<void*> Malloc(size_t size) override;
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override;
  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override;
  absl::Status Free(void* ptr) override;

 private:
  class TestCorrectness* fixture_;
  TestMetadataAlloc allocator_;
  uint32_t validate_every_n_;
  uint64_t iter_ = 0;
};

class TestCorrectness : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = (1 << 19);

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

  ~TestCorrectness() override {
    ckmalloc::TestGlobalMetadataAlloc::ClearAllocatorOverride();
  }

  ckmalloc::MetadataManagerFixture::TestMetadataManager& MetadataManager() {
    return metadata_manager_fixture_->MetadataManager();
  }

  ckmalloc::MainAllocatorFixture::TestMainAllocator& MainAllocator() {
    return main_allocator_fixture_->MainAllocator();
  }

  absl::Status RunTrace(const std::string& trace,
                        uint32_t validate_every_n = 1) {
    DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(trace));
    TestCkMalloc test(std::move(reader), this, validate_every_n);
    return test.Run();
  }

  absl::Status ValidateHeap() const {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(metadata_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateHeap());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() const {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateEmpty());
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

ckmalloc::Slab* TestMetadataAlloc::SlabAlloc() {
  return fixture_->MetadataManager().NewSlabMeta();
}

void TestMetadataAlloc::SlabFree(ckmalloc::MappedSlab* slab) {
  fixture_->MetadataManager().FreeSlabMeta(slab);
}

void* TestMetadataAlloc::Alloc(size_t size, size_t alignment) {
  return fixture_->MetadataManager().Alloc(size, alignment);
}

void TestMetadataAlloc::ClearAllAllocs() {}

void TestCkMalloc::InitializeHeap() {
  ckmalloc::TestGlobalMetadataAlloc::OverrideAllocator(&allocator_);
}

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

  if (++iter_ % validate_every_n_ == 0) {
    RETURN_IF_ERROR(fixture_->ValidateHeap());
  }
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

  if (++iter_ % validate_every_n_ == 0) {
    RETURN_IF_ERROR(fixture_->ValidateHeap());
  }
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

  if (++iter_ % validate_every_n_ == 0) {
    RETURN_IF_ERROR(fixture_->ValidateHeap());
  }
  return absl::OkStatus();
}

TEST_F(TestCorrectness, bddaa32) {
  ASSERT_THAT(RunTrace("traces/bdd-aa32.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, bddaa4) {
  ASSERT_THAT(RunTrace("traces/bdd-aa4.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, bddma4) {
  ASSERT_THAT(RunTrace("traces/bdd-ma4.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, bddnq7) {
  ASSERT_THAT(RunTrace("traces/bdd-nq7.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, cbitabs) {
  ASSERT_THAT(RunTrace("traces/cbit-abs.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, cbitparity) {
  ASSERT_THAT(RunTrace("traces/cbit-parity.trace",
                       /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, cbitsatadd) {
  ASSERT_THAT(RunTrace("traces/cbit-satadd.trace",
                       /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, cbitxyz) {
  ASSERT_THAT(RunTrace("traces/cbit-xyz.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, NgramFox1) {
  ASSERT_THAT(RunTrace("traces/ngram-fox1.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, NgramGulliver1) {
  ASSERT_THAT(
      RunTrace("traces/ngram-gulliver1.trace", /*validate_every_n=*/1024),
      util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, NgramGulliver2) {
  ASSERT_THAT(
      RunTrace("traces/ngram-gulliver2.trace", /*validate_every_n=*/1024),
      util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, NgramMoby1) {
  ASSERT_THAT(RunTrace("traces/ngram-moby1.trace",
                       /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, NgramShake1) {
  ASSERT_THAT(RunTrace("traces/ngram-shake1.trace",
                       /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynArray) {
  ASSERT_THAT(RunTrace("traces/syn-array.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynArrayShort) {
  ASSERT_THAT(RunTrace("traces/syn-array-short.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynMix) {
  ASSERT_THAT(RunTrace("traces/syn-mix.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynMixRealloc) {
  ASSERT_THAT(
      RunTrace("traces/syn-mix-realloc.trace", /*validate_every_n=*/1024),
      util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynMixShort) {
  ASSERT_THAT(RunTrace("traces/syn-mix-short.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynString) {
  ASSERT_THAT(RunTrace("traces/syn-string.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynStringShort) {
  ASSERT_THAT(RunTrace("traces/syn-string-short.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynStruct) {
  ASSERT_THAT(RunTrace("traces/syn-struct.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SynStructShort) {
  ASSERT_THAT(RunTrace("traces/syn-struct-short.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Test) {
  ASSERT_THAT(RunTrace("traces/test.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Server) {
  ASSERT_THAT(RunTrace("traces/server.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Simple) {
  ASSERT_THAT(RunTrace("traces/simple.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SimpleCalloc) {
  ASSERT_THAT(RunTrace("traces/simple_calloc.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, SimpleRealloc) {
  ASSERT_THAT(RunTrace("traces/simple_realloc.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Onoro) {
  ASSERT_THAT(RunTrace("traces/onoro.trace", /*validate_every_n=*/16384),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, OnoroCC) {
  ASSERT_THAT(RunTrace("traces/onoro-cc.trace", /*validate_every_n=*/4096),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Zero) {
  ASSERT_THAT(RunTrace("traces/test-zero.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

}  // namespace bench
