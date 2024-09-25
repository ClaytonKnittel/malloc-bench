#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/large_allocator_test_fixture.h"
#include "src/ckmalloc/main_allocator_test_fixture.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using testing::ElementsAre;
using util::IsOk;

class MainAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  template <typename T>
  static void Noop(T* val) {
    (void) val;
  }

  MainAllocatorTest()
      : heap_factory_(std::make_shared<TestHeapFactory>(kNumPages * kPageSize,
                                                        kNumPages * kPageSize)),
        metadata_heap_(
            static_cast<TestHeap*>(heap_factory_->Instances().begin()->get()),
            Noop<TestHeap>),
        main_heap_(static_cast<TestHeap*>(
                       (++heap_factory_->Instances().begin())->get()),
                   Noop<TestHeap>),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(
            std::make_shared<SlabManagerFixture>(main_heap_, slab_map_)),
        metadata_manager_fixture_(std::make_shared<MetadataManagerFixture>(
            metadata_heap_, slab_map_)),
        freelist_(std::make_shared<class Freelist>()),
        small_allocator_fixture_(std::make_shared<SmallAllocatorFixture>(
            main_heap_, slab_map_, slab_manager_fixture_, freelist_)),
        large_allocator_fixture_(std::make_shared<LargeAllocatorFixture>(
            main_heap_, slab_map_, slab_manager_fixture_, freelist_)),
        main_allocator_fixture_(std::make_shared<MainAllocatorFixture>(
            main_heap_, slab_map_, slab_manager_fixture_,
            metadata_manager_fixture_, small_allocator_fixture_,
            large_allocator_fixture_)) {
    TestSysAlloc::NewInstance(heap_factory_.get());
  }

  TestHeapFactory& HeapFactory() {
    return *heap_factory_;
  }

  TestHeap& Heap() {
    return *main_heap_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_fixture_->SlabManager();
  }

  TestMetadataManager& MetadataManager() {
    return metadata_manager_fixture_->MetadataManager();
  }

  TestMainAllocator& MainAllocator() {
    return main_allocator_fixture_->MainAllocator();
  }

  MainAllocatorFixture& Fixture() {
    return *main_allocator_fixture_;
  }

  std::vector<const TrackedBlock*> FreelistList() const {
    return large_allocator_fixture_->FreelistList();
  }

  size_t FreelistSize() const {
    return large_allocator_fixture_->FreelistSize();
  }

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(metadata_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateHeap());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateHeap());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateEmpty());

    if (heap_factory_->Instances().size() != 2) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Expected empty heap, but found %zu heap instances "
          "(only the metadata and main heap should be remaining)",
          heap_factory_->Instances().size()));
    }
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestHeap> metadata_heap_;
  std::shared_ptr<TestHeap> main_heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<MetadataManagerFixture> metadata_manager_fixture_;
  std::shared_ptr<Freelist> freelist_;
  std::shared_ptr<SmallAllocatorFixture> small_allocator_fixture_;
  std::shared_ptr<LargeAllocatorFixture> large_allocator_fixture_;
  std::shared_ptr<MainAllocatorFixture> main_allocator_fixture_;
};

TEST_F(MainAllocatorTest, Empty) {
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocSmall) {
  MainAllocator().Alloc(50);
  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocManySmall) {
  for (uint64_t size = 1; size <= kMaxSmallSize; size++) {
    MainAllocator().Alloc(size);
  }

  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeSmall) {
  Void* ptr = MainAllocator().Alloc(60);
  MainAllocator().Free(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, FreeTwoSmall) {
  Void* ptr1 = MainAllocator().Alloc(10);
  MainAllocator().Alloc(10);
  MainAllocator().Free(ptr1);

  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocLarge) {
  MainAllocator().Alloc(500);
  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocVeryLarge) {
  MainAllocator().Alloc(472);
  MainAllocator().Alloc(kPageSize + 1);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocManyLarge) {
  for (uint64_t size = 400; size < 800; size += 20) {
    MainAllocator().Alloc(size);
  }

  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeLarge) {
  Void* ptr = MainAllocator().Alloc(500);
  MainAllocator().Free(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, FreeTwoLarge) {
  Void* ptr1 = MainAllocator().Alloc(500);
  MainAllocator().Alloc(1000);
  MainAllocator().Free(ptr1);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 2);
}

TEST_F(MainAllocatorTest, ReallocOnce) {
  Void* ptr1 = MainAllocator().Alloc(500);
  Void* ptr2 = MainAllocator().Realloc(ptr1, 1000);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 1);
}

TEST_F(MainAllocatorTest, ReallocSmaller) {
  Void* ptr1 = MainAllocator().Alloc(500);
  Void* ptr2 = MainAllocator().Realloc(ptr1, 260);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 1);
}

TEST_F(MainAllocatorTest, ReallocMove) {
  Void* ptr1 = MainAllocator().Alloc(500);
  MainAllocator().Alloc(280);
  Void* ptr2 = MainAllocator().Realloc(ptr1, 550);

  EXPECT_NE(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 2);
}

TEST_F(MainAllocatorTest, AllocPagesizeMultiple) {
  MainAllocator().Alloc(kPageSize);
  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocSmallerThanPagesize) {
  MainAllocator().Alloc(kPageSize - 15);
  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocLargePagesizeMultiple) {
  MainAllocator().Alloc(14 * kPageSize);
  EXPECT_EQ(Heap().Size(), 14 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreePagesizeMultiple) {
  Void* ptr = MainAllocator().Alloc(kPageSize);
  MainAllocator().Free(ptr);

  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, ReallocPagesizeMultiple) {
  Void* ptr1 = MainAllocator().Alloc(4 * kPageSize);
  Void* ptr2 = MainAllocator().Realloc(ptr1, 2 * kPageSize);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_EQ(Heap().Size(), 4 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocHuge) {
  MainAllocator().Alloc(kMinMmapSize);

  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeHuge) {
  Void* ptr = MainAllocator().Alloc(kMinMmapSize);
  MainAllocator().Free(ptr);

  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, ReallocHugeToSmall) {
  Void* ptr1 = MainAllocator().Alloc(kMinMmapSize);
  MainAllocator().Realloc(ptr1, 64);

  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(HeapFactory().Instances().size(), 2);
}

TEST_F(MainAllocatorTest, ReallocHugeToLarge) {
  Void* ptr1 = MainAllocator().Alloc(kMinMmapSize);
  MainAllocator().Realloc(ptr1, 1024);

  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(HeapFactory().Instances().size(), 2);
}

TEST_F(MainAllocatorTest, ReallocHugeToHuge) {
  Void* ptr1 = MainAllocator().Alloc(kMinMmapSize);
  Void* ptr2 = MainAllocator().Realloc(ptr1, kMinMmapSize + 1);

  EXPECT_NE(ptr1, ptr2);
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(HeapFactory().Instances().size(), 3);
}

TEST_F(MainAllocatorTest, ReallocHugeToEqualHuge) {
  Void* ptr1 = MainAllocator().Alloc(kMinMmapSize + kPageSize);
  Void* ptr2 = MainAllocator().Realloc(ptr1, kMinMmapSize + kPageSize - 1);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(HeapFactory().Instances().size(), 3);
}

}  // namespace ckmalloc
