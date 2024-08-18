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

namespace ckmalloc {

using testing::ElementsAre;
using util::IsOk;

class MainAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  MainAllocatorTest()
      : heap_(std::make_shared<TestHeap>(kNumPages)),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(
            std::make_shared<SlabManagerFixture>(heap_, slab_map_)),
        small_allocator_fixture_(std::make_shared<SmallAllocatorFixture>(
            heap_, slab_map_, slab_manager_fixture_)),
        main_allocator_fixture_(std::make_shared<MainAllocatorFixture>(
            heap_, slab_map_, slab_manager_fixture_,
            small_allocator_fixture_)) {}

  TestHeap& Heap() {
    return slab_manager_fixture_->Heap();
  }

  SlabManagerFixture::TestSlabManager& SlabManager() {
    return slab_manager_fixture_->SlabManager();
  }

  MainAllocatorFixture::TestMainAllocator& MainAllocator() {
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
  std::shared_ptr<TestHeap> heap_;
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
  void* ptr = MainAllocator().Alloc(60);
  MainAllocator().Free(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, FreeTwoSmall) {
  void* ptr1 = MainAllocator().Alloc(10);
  MainAllocator().Alloc(10);
  MainAllocator().Free(ptr1);

  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocLarge) {
  MainAllocator().Alloc(500);
  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, AllocHuge) {
  MainAllocator().Alloc(472);
  MainAllocator().Alloc(kPageSize);
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
  void* ptr = MainAllocator().Alloc(500);
  MainAllocator().Free(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(MainAllocatorTest, FreeTwoLarge) {
  void* ptr1 = MainAllocator().Alloc(500);
  MainAllocator().Alloc(1000);
  MainAllocator().Free(ptr1);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 2);
}

TEST_F(MainAllocatorTest, ReallocOnce) {
  void* ptr1 = MainAllocator().Alloc(500);
  void* ptr2 = MainAllocator().Realloc(ptr1, 1000);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 1);
}

TEST_F(MainAllocatorTest, ReallocSmaller) {
  void* ptr1 = MainAllocator().Alloc(500);
  void* ptr2 = MainAllocator().Realloc(ptr1, 200);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 1);
}

TEST_F(MainAllocatorTest, ReallocMove) {
  void* ptr1 = MainAllocator().Alloc(500);
  MainAllocator().Alloc(200);
  void* ptr2 = MainAllocator().Realloc(ptr1, 550);

  EXPECT_NE(ptr1, ptr2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(FreelistSize(), 2);
}

}  // namespace ckmalloc
