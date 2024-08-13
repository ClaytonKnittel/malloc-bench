#include "src/ckmalloc/main_allocator.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/main_allocator_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"

namespace ckmalloc {

using testing::ElementsAre;
using testing::Pointee;
using testing::Property;
using util::IsOk;

class MainAllocatorTest : public ::testing::Test {
 public:
  MainAllocatorTest()
      : slab_manager_fixture_(std::make_shared<SlabManagerFixture>()),
        main_allocator_fixture_(std::make_shared<MainAllocatorFixture>(
            slab_manager_fixture_->HeapPtr(),
            slab_manager_fixture_->SlabMapPtr(), slab_manager_fixture_,
            slab_manager_fixture_->SlabManagerPtr())) {}

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

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(main_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<MainAllocatorFixture> main_allocator_fixture_;
};

TEST_F(MainAllocatorTest, TestEmpty) {
  EXPECT_EQ(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, TestAllocLarge) {
  MainAllocator().Alloc(500);
  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, TestAllocManyLarge) {
  for (uint64_t size = 400; size < 800; size += 20) {
    MainAllocator().Alloc(size);
  }

  EXPECT_NE(Heap().Size(), 0);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MainAllocatorTest, TestFreeLarge) {
  void* ptr = MainAllocator().Alloc(500);
  MainAllocator().Free(ptr);

  EXPECT_THAT(ValidateHeap(), IsOk());

  for (const auto& x : FreelistList()) {
    std::cout << reinterpret_cast<const void*>(&x) << " " << x.Size()
              << std::endl;
  }
  EXPECT_THAT(FreelistList(), ElementsAre());
}

}  // namespace ckmalloc
