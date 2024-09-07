#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/main_allocator_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/heap_interface.h"

namespace ckmalloc {

using testing::ElementsAre;
using testing::Pointee;
using testing::Property;
using util::IsOk;

class MainAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  MainAllocatorTest()
      : heap_factory_(std::make_shared<TestHeapFactory>(kNumPages * kPageSize)),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(std::make_shared<SlabManagerFixture>(
            heap_factory_, slab_map_, /*heap_idx=*/0)),
        small_allocator_fixture_(std::make_shared<SmallAllocatorFixture>(
            heap_factory_, slab_map_, slab_manager_fixture_)),
        main_allocator_fixture_(std::make_shared<MainAllocatorFixture>(
            heap_factory_, slab_map_, slab_manager_fixture_,
            small_allocator_fixture_)) {}

  TestHeapFactory& HeapFactory() {
    return *heap_factory_;
  }

  bench::Heap& Heap() {
    return *HeapFactory().Instance(0);
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_fixture_->SlabManager();
  }

  TestMainAllocator& MainAllocator() {
    return main_allocator_fixture_->MainAllocator();
  }

  MainAllocatorFixture& Fixture() {
    return *main_allocator_fixture_;
  }

  LinkedList<TrackedBlock>& FreelistList() {
    return main_allocator_fixture_->MainAllocator().Freelist().free_blocks_;
  }

  size_t FreelistSize() {
    return absl::c_count_if(
        main_allocator_fixture_->MainAllocator().Freelist().free_blocks_,
        [](const auto&) { return true; });
  }

  void* Alloc(size_t user_size) {
    return MainAllocator().Alloc(user_size);
  }

  void* Realloc(void* ptr, size_t user_size) {
    return MainAllocator().Realloc(ptr, user_size);
  }

  void Free(void* ptr) {
    MainAllocator().Free(ptr);
  }

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateHeap());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(small_allocator_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateEmpty());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<SmallAllocatorFixture> small_allocator_fixture_;
  std::shared_ptr<MainAllocatorFixture> main_allocator_fixture_;
};

TEST_F(MainAllocatorTest, Empty) {
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocSmall) {
  Alloc(50);
  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocManySmall) {
  for (uint64_t size = 1; size <= kMaxSmallSize; size++) {
    Alloc(size);
  }

  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeSmall) {
  void* ptr = Alloc(60);
  Free(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, FreeTwoSmall) {
  void* ptr1 = Alloc(10);
  Alloc(10);
  Free(ptr1);

  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocLarge) {
  Alloc(500);
  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocHuge) {
  Alloc(472);
  Alloc(kPageSize + 1);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocManyLarge) {
  for (uint64_t size = 400; size < 800; size += 20) {
    Alloc(size);
  }

  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeLarge) {
  void* ptr = Alloc(500);
  Free(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, FreeTwoLarge) {
  void* ptr1 = Alloc(500);
  Alloc(1000);
  Free(ptr1);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 2);
}

TEST_F(MainAllocatorTest, ReallocOnce) {
  void* ptr1 = Alloc(500);
  void* ptr2 = Realloc(ptr1, 1000);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 1);
}

TEST_F(MainAllocatorTest, ReallocSmaller) {
  void* ptr1 = Alloc(500);
  void* ptr2 = Realloc(ptr1, 200);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 1);
}

TEST_F(MainAllocatorTest, ReallocMove) {
  void* ptr1 = Alloc(500);
  Alloc(200);
  void* ptr2 = Realloc(ptr1, 550);

  EXPECT_NE(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 2);
}

TEST_F(MainAllocatorTest, FreeCarveBeginning) {
  void* ptr1 = Alloc(6000);
  void* ptr2 = Alloc(160);
  Free(ptr1);

  MappedSlab* right_slab =
      SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr2));
  ASSERT_THAT(right_slab, Pointee(Property(&Slab::Type, SlabType::kBlocked)));
  EXPECT_EQ(right_slab->Pages(), 1);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeCarveBeginningSmall) {
  void* ptr1 = Alloc(6200);
  void* ptr2 = Alloc(160);
  void* ptr3 = Realloc(ptr1, 4000);
  void* ptr4 = Alloc(200);
  Free(ptr3);
  Free(ptr4);

  MappedSlab* right_slab =
      SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr2));
  ASSERT_THAT(right_slab, Pointee(Property(&Slab::Type, SlabType::kBlocked)));
  EXPECT_EQ(right_slab->Pages(), 1);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeCarveBeginningLarge) {
  void* ptr1 = Alloc(9904);
  void* ptr2 = Alloc(840);
  Free(ptr1);

  MappedSlab* right_slab =
      SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr2));
  ASSERT_THAT(right_slab, Pointee(Property(&Slab::Type, SlabType::kBlocked)));
  EXPECT_EQ(right_slab->Pages(), 1);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeCarveEnd) {
  void* ptr1 = Alloc(6000);
  void* ptr2 = Alloc(160);
  void* ptr3 = Realloc(ptr1, 160);
  Free(ptr2);

  MappedSlab* slab = SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr3));
  ASSERT_THAT(slab, Pointee(Property(&Slab::Type, SlabType::kBlocked)));
  EXPECT_EQ(slab->Pages(), 1);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeCarveEndExactMultiple) {
  void* ptr1 = Alloc(6000);
  void* ptr2 = Alloc(160);
  void* ptr3 = Realloc(ptr1, 4072);
  Free(ptr2);

  MappedSlab* slab = SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr3));
  ASSERT_THAT(slab, Pointee(Property(&Slab::Type, SlabType::kBlocked)));
  EXPECT_EQ(slab->Pages(), 1);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreeCarveCenterEnd) {
  void* ptr1 = Alloc(12000);
  void* ptr2 = Alloc(160);
  void* ptr3 = Realloc(ptr1, 6000);
  void* ptr4 = Alloc(160);
  void* ptr5 = Realloc(ptr3, 160);
  Free(ptr4);

  MappedSlab* left_slab = SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr5));
  MappedSlab* middle_slab =
      SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr4));
  MappedSlab* right_slab =
      SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr2));
  ASSERT_THAT(left_slab, Pointee(Property(&Slab::Type, SlabType::kBlocked)));
  ASSERT_THAT(middle_slab, Pointee(Property(&Slab::Type, SlabType::kFree)));
  ASSERT_THAT(right_slab, Pointee(Property(&Slab::Type, SlabType::kBlocked)));
  EXPECT_EQ(left_slab->Pages(), 1);
  EXPECT_EQ(middle_slab->Pages(), 1);
  EXPECT_EQ(right_slab->Pages(), 1);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocPagesizeMultiple) {
  Alloc(kPageSize);
  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocSmallerThanPagesize) {
  Alloc(kPageSize - 15);
  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocLargePagesizeMultiple) {
  Alloc(14 * kPageSize);
  EXPECT_EQ(Heap().Size(), 14 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, FreePagesizeMultiple) {
  void* ptr = Alloc(kPageSize);
  Free(ptr);

  EXPECT_EQ(Heap().Size(), kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, AllocSinglePageFromFreeSlabs) {
  void* ptr1 = Alloc(512);
  Free(ptr1);
  Alloc(kPageSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), kPageSize);
}

TEST_F(MainAllocatorTest, ReallocPagesizeMultiple) {
  void* ptr1 = Alloc(2 * kPageSize);
  void* ptr2 = Realloc(ptr1, kPageSize);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, ReallocExtendPagesizeMultiple) {
  void* ptr1 = Alloc(2 * kPageSize);
  Free(ptr1);
  void* ptr2 = Alloc(kPageSize);
  void* ptr3 = Realloc(ptr2, 2 * kPageSize);

  EXPECT_EQ(ptr2, ptr3);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, ReallocExtendHeapPagesizeMultiple) {
  void* ptr1 = Alloc(kPageSize);
  void* ptr2 = Realloc(ptr1, 2 * kPageSize);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, ReallocExtendHeapWithFreeSlabPagesizeMultiple) {
  void* ptr1 = Alloc(3 * kPageSize);
  Free(ptr1);
  void* ptr2 = Alloc(kPageSize);
  void* ptr3 = Realloc(ptr2, 4 * kPageSize);

  EXPECT_EQ(ptr2, ptr3);
  EXPECT_EQ(Heap().Size(), 4 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocLargeBlockIntoNextFreeSlab) {
  void* ptr1 = Alloc(6000);
  Realloc(ptr1, 500);
  Alloc(4200);

  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest,
       AllocLargeBlockIntoNextFreeSlabAndNextLargeSlabAfter) {
  Alloc(3500);
  void* ptr2 = Alloc(8);
  void* ptr3 = Alloc(3000);
  Alloc(1000);
  Free(ptr2);
  Free(ptr3);
  Alloc(7600);

  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

}  // namespace ckmalloc
