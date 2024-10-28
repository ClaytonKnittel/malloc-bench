#include <cstdlib>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/large_allocator_test_fixture.h"
#include "src/ckmalloc/main_allocator_test_fixture.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"
#include "src/heap_factory.h"
#include "src/tracefile_executor.h"
#include "src/tracefile_reader.h"

namespace bench {

using util::IsOk;

// #define PRINT

class TestCkMalloc : public TracefileExecutor {
  friend class TestCorrectness;

 public:
  explicit TestCkMalloc(TracefileReader&& tracefile_reader,
                        HeapFactory& heap_factory,
                        class TestCorrectness* fixture,
                        uint32_t validate_every_n)
      : TracefileExecutor(tracefile_reader, heap_factory),
        fixture_(fixture),
        validate_every_n_(validate_every_n) {}

  void InitializeHeap(HeapFactory& heap_factory) override {}

  absl::StatusOr<void*> Malloc(size_t size,
                               std::optional<size_t> alignment) override;
  absl::StatusOr<void*> Calloc(size_t nmemb, size_t size) override;
  absl::StatusOr<void*> Realloc(void* ptr, size_t size) override;
  absl::Status Free(void* ptr, std::optional<size_t> size_hint,
                    std::optional<size_t> alignment_hint) override;

 private:
  class TestCorrectness* fixture_;
  uint32_t validate_every_n_;
  uint64_t iter_ = 0;
};

class TestCorrectness : public ::testing::Test {
 public:
  TestCorrectness()
      : heap_factory_(std::make_shared<ckmalloc::TestHeapFactory>(
            ckmalloc::kMetadataHeapSize)),
        metadata_heap_(static_cast<ckmalloc::TestHeap*>(
                           heap_factory_->Instances().begin()->get()),
                       ckmalloc::Noop<ckmalloc::TestHeap>),
        slab_map_(std::make_shared<ckmalloc::TestSlabMap>()),
        slab_manager_fixture_(std::make_shared<ckmalloc::SlabManagerFixture>(
            heap_factory_, slab_map_, ckmalloc::kUserHeapSize)),
        metadata_manager_fixture_(
            std::make_shared<ckmalloc::MetadataManagerFixture>(
                metadata_heap_, slab_map_, ckmalloc::kMetadataHeapSize)),
        freelist_(std::make_shared<ckmalloc::Freelist>()),
        small_allocator_fixture_(
            std::make_shared<ckmalloc::SmallAllocatorFixture>(
                slab_map_, slab_manager_fixture_, freelist_)),
        large_allocator_fixture_(
            std::make_shared<ckmalloc::LargeAllocatorFixture>(
                slab_map_, slab_manager_fixture_, freelist_)),
        main_allocator_fixture_(
            std::make_shared<ckmalloc::MainAllocatorFixture>(
                slab_map_, slab_manager_fixture_, metadata_manager_fixture_,
                small_allocator_fixture_, large_allocator_fixture_)) {
    ckmalloc::TestSysAlloc::NewInstance(heap_factory_.get());
  }

  ~TestCorrectness() override {
    ckmalloc::TestSysAlloc::Reset();
  }

  ckmalloc::TestMetadataManager& MetadataManager() {
    return metadata_manager_fixture_->MetadataManager();
  }

  ckmalloc::TestMainAllocator& MainAllocator() {
    return main_allocator_fixture_->MainAllocator();
  }

  absl::Status RunTrace(const std::string& trace,
                        uint32_t validate_every_n = 1) {
    DEFINE_OR_RETURN(TracefileReader, reader, TracefileReader::Open(trace));
    TestCkMalloc test(std::move(reader), *heap_factory_, this,
                      validate_every_n);
    return test.Run();
  }

  absl::Status ValidateHeap() const {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(metadata_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateHeap());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateHeap());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() const {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateEmpty());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<ckmalloc::TestHeapFactory> heap_factory_;
  std::shared_ptr<ckmalloc::TestHeap> metadata_heap_;
  std::shared_ptr<ckmalloc::TestSlabMap> slab_map_;
  std::shared_ptr<ckmalloc::SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<ckmalloc::MetadataManagerFixture> metadata_manager_fixture_;
  std::shared_ptr<ckmalloc::Freelist> freelist_;
  std::shared_ptr<ckmalloc::SmallAllocatorFixture> small_allocator_fixture_;
  std::shared_ptr<ckmalloc::LargeAllocatorFixture> large_allocator_fixture_;
  std::shared_ptr<ckmalloc::MainAllocatorFixture> main_allocator_fixture_;
};

absl::StatusOr<void*> TestCkMalloc::Malloc(size_t size,
                                           std::optional<size_t> alignment) {
  (void) alignment;
  if (size == 0) {
    return nullptr;
  }

#ifdef PRINT
  std::cout << "malloc(" << size << ") (" << iter_ << ")" << std::endl;
#endif
  void* result = alignment.has_value() ? fixture_->MainAllocator().AlignedAlloc(
                                             size, alignment.value())
                                       : fixture_->MainAllocator().Alloc(size);
#ifdef PRINT
  std::cout << "returned " << result << std::endl;
#endif
  if (result == nullptr) {
    return absl::FailedPreconditionError("Returned nullptr from malloc");
  }

  if (alignment.has_value() &&
      !ckmalloc::IsAligned(reinterpret_cast<size_t>(result),
                           alignment.value())) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Pointer %p of size %zu not aligned to %zu", result,
                        size, alignment.value()));
  }

  if (++iter_ % validate_every_n_ == 0) {
    RETURN_IF_ERROR(fixture_->ValidateHeap());
  }
  return result;
}

absl::StatusOr<void*> TestCkMalloc::Calloc(size_t nmemb, size_t size) {
  return Malloc(nmemb * size, /*alignment=*/std::nullopt);
}

absl::StatusOr<void*> TestCkMalloc::Realloc(void* ptr, size_t size) {
  if (ptr == nullptr) {
    return Malloc(size, /*alignment=*/std::nullopt);
  }

  CK_ASSERT_NE(size, 0);
#ifdef PRINT
  std::cout << "realloc(" << ptr << ", " << size << ") (" << iter_ << ")"
            << std::endl;
#endif
  void* result = fixture_->MainAllocator().Realloc(
      reinterpret_cast<ckmalloc::Void*>(ptr), size);
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

absl::Status TestCkMalloc::Free(void* ptr, std::optional<size_t> size_hint,
                                std::optional<size_t> alignment_hint) {
  (void) size_hint;
  (void) alignment_hint;
  if (ptr == nullptr) {
    return absl::OkStatus();
  }

#ifdef PRINT
  std::cout << "free(" << ptr << ") (" << iter_ << ")" << std::endl;
#endif
  fixture_->MainAllocator().Free(reinterpret_cast<ckmalloc::Void*>(ptr));

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

TEST_F(TestCorrectness, Clang) {
  ASSERT_THAT(RunTrace("traces/clang.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Firefox) {
  ASSERT_THAT(RunTrace("traces/firefox.trace", /*validate_every_n=*/16384),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, FourInARow) {
  ASSERT_THAT(
      RunTrace("traces/four-in-a-row.trace", /*validate_every_n=*/16384),
      util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Grep) {
  ASSERT_THAT(RunTrace("traces/grep.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Gto) {
  ASSERT_THAT(RunTrace("traces/gto.trace", /*validate_every_n=*/1 << 19),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, HaskellWebServer) {
  ASSERT_THAT(RunTrace("traces/haskell-web-server.trace",
                       /*validate_every_n=*/16384),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, McServerSmall) {
  ASSERT_THAT(
      RunTrace("traces/mc_server_small.trace", /*validate_every_n=*/1024),
      util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, McServer) {
  ASSERT_THAT(RunTrace("traces/mc_server.trace", /*validate_every_n=*/16384),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, McServerLarge) {
  ASSERT_THAT(
      RunTrace("traces/mc_server_large.trace", /*validate_every_n=*/16384),
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

TEST_F(TestCorrectness, PyCatanAi) {
  ASSERT_THAT(RunTrace("traces/py-catan-ai.trace",
                       /*validate_every_n=*/16384),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, PyEulerNayuki) {
  ASSERT_THAT(
      RunTrace("traces/py-euler-nayuki.trace", /*validate_every_n=*/16384),
      util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Scp) {
  ASSERT_THAT(RunTrace("traces/scp.trace", /*validate_every_n=*/4096),
              util::IsOk());
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

TEST_F(TestCorrectness, Solitaire) {
  ASSERT_THAT(RunTrace("traces/solitaire.trace", /*validate_every_n=*/1024),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Ssh) {
  ASSERT_THAT(RunTrace("traces/ssh.trace", /*validate_every_n=*/1024),
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

TEST_F(TestCorrectness, TestZero) {
  ASSERT_THAT(RunTrace("traces/test-zero.trace"), util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Vim) {
  ASSERT_THAT(RunTrace("traces/vim.trace", /*validate_every_n=*/65535),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(TestCorrectness, Vlc) {
  ASSERT_THAT(RunTrace("traces/vlc.trace", /*validate_every_n=*/16384),
              util::IsOk());
  ASSERT_THAT(ValidateEmpty(), IsOk());
}

}  // namespace bench
