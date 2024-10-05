#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/large_allocator_test_fixture.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

using testing::AnyOf;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using util::IsOk;

// TODO: Refactor this test entirely.
class LargeAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kHeapSize = 64 * kPageSize;
  static constexpr uint64_t kSinglePageBlockSize =
      kPageSize - Block::kFirstBlockInSlabOffset - Block::kMetadataOverhead;

  LargeAllocatorTest()
      : heap_factory_(std::make_shared<TestHeapFactory>()),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(std::make_shared<SlabManagerFixture>(
            heap_factory_, slab_map_, kHeapSize)),
        freelist_(std::make_shared<class Freelist>()),
        large_allocator_fixture_(std::make_shared<LargeAllocatorFixture>(
            slab_map_, slab_manager_fixture_, freelist_)) {
    TestSysAlloc::NewInstance(heap_factory_.get());
  }

  ~LargeAllocatorTest() override {
    TestSysAlloc::Reset();
  }

  static bool PrevFree(const Block* block) {
    return block->PrevFree();
  }

  static void WriteFooterAndPrevFree(Block* block) {
    block->WriteFooterAndPrevFree();
  }

  TestHeap& Heap() {
    auto heaps = slab_manager_fixture_->Heaps();
    CK_ASSERT_EQ(std::ranges::distance(heaps.begin(), heaps.end()), 1);
    return *heaps.begin()->second.second;
  }

  TestSlabMap& SlabMap() {
    return slab_manager_fixture_->SlabMap();
  }

  TestSlabManager& SlabManager() {
    return slab_manager_fixture_->SlabManager();
  }

  TestLargeAllocator& LargeAllocator() {
    return large_allocator_fixture_->LargeAllocator();
  }

  AllocatedBlock* Alloc(uint64_t block_size) {
    Void* res =
        LargeAllocator().AllocLarge(Block::UserSizeForBlockSize(block_size));
    return res != nullptr ? AllocatedBlock::FromUserDataPtr(res) : nullptr;
  }

  AllocatedBlock* Realloc(AllocatedBlock* block, uint64_t block_size) {
    LargeSlab* slab = SlabMap().FindSlab(PageId::FromPtr(block))->ToLarge();
    Void* res = LargeAllocator().ReallocLarge(
        slab, block->UserDataPtr(), Block::UserSizeForBlockSize(block_size));
    return res != nullptr ? AllocatedBlock::FromUserDataPtr(res) : nullptr;
  }

  void Free(AllocatedBlock* block) {
    LargeSlab* slab = SlabMap().FindSlab(PageId::FromPtr(block))->ToLarge();
    LargeAllocator().FreeLarge(slab, block->UserDataPtr());
  }

  Freelist& Freelist() {
    return large_allocator_fixture_->Freelist();
  }

  Block* FindFree(uint64_t block_size) {
    return static_cast<Block*>(Freelist().FindFree(block_size));
  }

  std::vector<const TrackedBlock*> FreelistList() {
    return large_allocator_fixture_->FreelistList();
  }

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateEmpty());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<class Freelist> freelist_;
  std::shared_ptr<LargeAllocatorFixture> large_allocator_fixture_;
};

TEST_F(LargeAllocatorTest, FreeBlock) {
  constexpr size_t kBlockSize = 0xca90;
  uint64_t
      region[(kBlockSize + Block::kMetadataOverhead) / sizeof(uint64_t)] = {};
  Block* block = reinterpret_cast<Block*>(&region[0]);

  Freelist().InitFree(block, kBlockSize);
  EXPECT_TRUE(block->Free());
  EXPECT_EQ(block->Size(), kBlockSize);
  EXPECT_FALSE(PrevFree(block));

  Block* next_adjacent =
      reinterpret_cast<Block*>(&region[kBlockSize / sizeof(uint64_t)]);
  EXPECT_EQ(block->NextAdjacentBlock(), next_adjacent);
  ASSERT_TRUE(PrevFree(next_adjacent));
  EXPECT_EQ(next_adjacent->PrevAdjacentBlock(), block);

  EXPECT_EQ(block->UserDataSize(), kBlockSize - Block::kMetadataOverhead);
  EXPECT_TRUE(block->IsTracked());

  // This should not trigger an assertion failure.
  block->ToTracked();
}

TEST_F(LargeAllocatorTest, UntrackedBlock) {
  constexpr size_t kBlockSize = 0x40;
  struct {
    UntrackedBlock free_block;
    uint8_t data[kBlockSize + Block::kMetadataOverhead];
  } data;
  Block* block = &data.free_block;

  Freelist().InitFree(block, kBlockSize);
  EXPECT_TRUE(block->Free());
  EXPECT_EQ(block->Size(), kBlockSize);
  EXPECT_FALSE(PrevFree(block));

  EXPECT_EQ(PtrDistance(block->NextAdjacentBlock(), block), kBlockSize);

  EXPECT_TRUE(block->IsUntracked());

  // This should not cause assertion failure.
  block->ToUntracked();

  // Untracked blocks do not go in the freelist.
  EXPECT_THAT(FreelistList(), ElementsAre());
}

TEST_F(LargeAllocatorTest, PrevFree) {
  constexpr size_t kBlockSize = 0x1030;
  uint64_t
      region[(kBlockSize + Block::kMetadataOverhead) / sizeof(uint64_t)] = {};
  Block* block =
      reinterpret_cast<Block*>(&region[kBlockSize / sizeof(uint64_t)]);
  Block* prev = reinterpret_cast<Block*>(&region[0]);

  block->InitAllocated(0x54540, /*prev_free=*/false);

  // This should write to the footer of `block`, and set block's prev_free bit.
  Freelist().InitFree(prev, kBlockSize);

  EXPECT_TRUE(PrevFree(block));
  EXPECT_EQ(block->PrevAdjacentBlock(), prev);
}

TEST_F(LargeAllocatorTest, Empty) {
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(FindFree(Block::kMinTrackedSize), nullptr);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(LargeAllocatorTest, OnlyAllocatedAndUntracked) {
  Alloc(0x140);

  AllocatedBlock* b2 = Alloc(0x380);
  Free(b2);
  AllocatedBlock* b3 = Alloc(0x300);
  EXPECT_EQ(b2, b3);

  AllocatedBlock* b4 = Alloc(0x200);

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre(b4->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, OneFree) {
  constexpr uint64_t kSize = 0x110;

  AllocatedBlock* b1 = Alloc(kSize);
  Alloc(kSinglePageBlockSize - kSize);
  Free(b1);

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre(b1->ToTracked()));
  EXPECT_EQ(FindFree(kSize), b1);
  EXPECT_EQ(FindFree(kSize + kDefaultAlignment), nullptr);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ManyFree) {
  Alloc(0x110);
  AllocatedBlock* b1 = Alloc(0x500);
  Alloc(kSinglePageBlockSize - 0x110 - 0x500);
  AllocatedBlock* b2 = Alloc(0x300);
  Alloc(kSinglePageBlockSize - 0x300);
  AllocatedBlock* b3 = Alloc(0x900);
  Alloc(kSinglePageBlockSize - 0x900);
  AllocatedBlock* b4 = Alloc(0x4B0);
  Alloc(kSinglePageBlockSize - 0x4B0);

  Free(b1);
  Free(b2);
  Free(b3);
  Free(b4);

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(b1->ToTracked(), b2->ToTracked(),
                                   b3->ToTracked(), b4->ToTracked()));

  EXPECT_EQ(FindFree(0x900), b3);
  EXPECT_EQ(FindFree(0x900 + kDefaultAlignment), nullptr);

  EXPECT_THAT(FindFree(0x500), AnyOf(b1, b3));

  EXPECT_THAT(FindFree(0x300), AnyOf(b1, b2, b3));

  EXPECT_THAT(FindFree(0x200), AnyOf(b1, b2, b3, b4));

  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, Split) {
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNewBlockSize = 0x130;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(0x150);
  Free(b1);
  AllocatedBlock* b3 = Alloc(kNewBlockSize);
  ASSERT_EQ(b1, b3);
  EXPECT_EQ(b3->Size(), kNewBlockSize);

  ASSERT_TRUE(b3->NextAdjacentBlock()->Free());
  TrackedBlock* next_free = b3->NextAdjacentBlock()->ToTracked();
  EXPECT_EQ(next_free->Size(), kBlockSize - kNewBlockSize);
  EXPECT_EQ(next_free->NextAdjacentBlock(), b2);
  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(next_free, b2->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, SplitWithMinBlockSizeRemainder) {
  constexpr uint64_t kBlockSize = 0xD30;
  constexpr uint64_t kNewBlockSize = 0xD10;
  static_assert(kBlockSize - kNewBlockSize == Block::kMinBlockSize);

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(0x150);
  Free(b1);
  AllocatedBlock* b3 = Alloc(kNewBlockSize);
  ASSERT_EQ(b1, b3);
  EXPECT_EQ(b3->Size(), kNewBlockSize);

  EXPECT_THAT(FreelistList(), ElementsAre(b2->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, SplitWithBelowMinBlockSizeRemainder) {
  constexpr uint64_t kBlockSize = 0xD30;
  constexpr uint64_t kNewBlockSize = 0xD20;
  static_assert(kBlockSize - kNewBlockSize < Block::kMinBlockSize);

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(0x150);
  Free(b1);
  AllocatedBlock* b3 = Alloc(kNewBlockSize);
  ASSERT_EQ(b1, b3);

  // The block should not be resized since it would leave a remaining free block
  // <= min block size.
  EXPECT_EQ(b3->Size(), kBlockSize);
  EXPECT_EQ(b3->NextAdjacentBlock(), b2);
  EXPECT_THAT(FreelistList(), ElementsAre(b2->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, FreeAsOnlyBlock) {
  constexpr uint64_t kBlockSize = 0xFF0;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  Free(b1);

  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(ValidateEmpty(), IsOk());
}

TEST_F(LargeAllocatorTest, FreeWithAllocatedNeighbors) {
  constexpr uint64_t kBlockSize = 0xD30;

  Alloc(0x140);
  AllocatedBlock* block = Alloc(kBlockSize);
  Alloc(0x180);

  Free(block);
  EXPECT_THAT(FreelistList(), ElementsAre(block->ToFree()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, FreeWithFreePrev) {
  constexpr uint64_t kPrevSize = 0x240;
  constexpr uint64_t kBlockSize = 0x5B0;

  AllocatedBlock* b1 = Alloc(kPrevSize);
  AllocatedBlock* b2 = Alloc(kBlockSize);
  AllocatedBlock* b3 = Alloc(0x160);
  Free(b1);
  Free(b2);

  EXPECT_EQ(b1->Size(), kPrevSize + kBlockSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(b1->ToTracked(), b3->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, FreeWithFreeNext) {
  constexpr uint64_t kBlockSize = 0x550;
  constexpr uint64_t kNextSize = 0x4A0;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(kNextSize);
  AllocatedBlock* b3 = Alloc(0x170);
  Free(b2);
  Free(b1);

  EXPECT_EQ(b1->Size(), kBlockSize + kNextSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(b1->ToTracked(), b3->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, FreeWithFreeNextAndPrev) {
  constexpr uint64_t kPrevSize = 0x150;
  constexpr uint64_t kBlockSize = 0x330;
  constexpr uint64_t kNextSize = 0x570;

  AllocatedBlock* b1 = Alloc(kPrevSize);
  AllocatedBlock* b2 = Alloc(kBlockSize);
  AllocatedBlock* b3 = Alloc(kNextSize);
  AllocatedBlock* b4 = Alloc(0x200);
  Free(b1);
  Free(b3);
  Free(b2);

  EXPECT_EQ(b1->Size(), kPrevSize + kBlockSize + kNextSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(b1->ToFree(), b4->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, FreeWithUntrackedNeighbors) {
  GTEST_SKIP() << "Skipping since untracked blocks will soon not exist.";

  constexpr uint64_t kPrevSize = 0x30;
  constexpr uint64_t kBlockSize = 0x510;
  constexpr uint64_t kNextSize = 0x80;

  AllocatedBlock* b1 = Alloc(kPrevSize);
  AllocatedBlock* b2 = Alloc(kBlockSize);
  Alloc(kNextSize);
  Alloc(0x200);
  EXPECT_THAT(FreelistList(), ElementsAre());

  Free(b2);
  EXPECT_EQ(b1->Size(), kPrevSize + kBlockSize + kNextSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(), ElementsAre(b1->ToFree()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeDown) {
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNewSize = 0x340;

  Alloc(0x140);
  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(0x200);

  AllocatedBlock* b3 = Realloc(b1, kNewSize);
  ASSERT_EQ(b3, b1);
  EXPECT_EQ(b1, b3);
  EXPECT_EQ(b3->Size(), kNewSize);

  Block* next = b3->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), b2);

  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(next, b2->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeDownBelowMinBlockSizeRemainder) {
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNewSize = 0x520;
  static_assert(kBlockSize - kNewSize < Block::kMinBlockSize);

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(0x200);

  AllocatedBlock* b3 = Realloc(b1, kNewSize);
  ASSERT_EQ(b3, b1);
  // The block can't change size since that would leave a remainder block < min
  // block size.
  EXPECT_EQ(b3->Size(), kBlockSize);

  Block* next = b3->NextAdjacentBlock();
  EXPECT_EQ(next, b2);

  EXPECT_THAT(FreelistList(), ElementsAre(b2->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeDownBeforeFree) {
  constexpr uint64_t kBlockSize = 0x290;
  constexpr uint64_t kNewSize = 0x130;
  constexpr uint64_t kNextSize = 0x140;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(kNextSize);
  AllocatedBlock* end_block = Alloc(0x150);
  Free(b2);

  AllocatedBlock* b3 = Realloc(b1, kNewSize);
  ASSERT_NE(b3, nullptr);
  EXPECT_EQ(b3, b1);
  EXPECT_EQ(b3->Size(), kNewSize);

  Block* next = b3->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize + kNextSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), end_block);

  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(next, end_block->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeAllocated) {
  constexpr uint64_t kBlockSize = 0x290;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  Alloc(0x200);

  Block* b3 = Realloc(b1, kBlockSize + kDefaultAlignment);
  // b2 should have been placed somewhere else since the block can't have
  // in-place upsized.
  ASSERT_NE(b3, b1);
  EXPECT_EQ(b1->Size(), kBlockSize);

  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(b1->ToFree(), b3->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFree) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x690;
  constexpr uint64_t kNextSize = 0x400;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(kNextSize);
  AllocatedBlock* b3 = Alloc(0x200);
  Free(b2);

  AllocatedBlock* b4 = Realloc(b1, kNewSize);
  ASSERT_EQ(b4, b1);
  EXPECT_EQ(b4->Size(), kNewSize);

  Block* next = b4->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize + kNextSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), b3);

  EXPECT_THAT(FreelistList(),
              UnorderedElementsAre(next, b3->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFreeLessThanMinSizeRemainder) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x680;
  constexpr uint64_t kNextSize = 0x200;
  static_assert(kNewSize < kBlockSize + kNextSize);
  static_assert(kBlockSize + kNextSize - kNewSize < Block::kMinBlockSize);

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(kNextSize);
  AllocatedBlock* b3 = Alloc(0x230);
  Free(b2);

  AllocatedBlock* b4 = Realloc(b1, kNewSize);
  ASSERT_EQ(b4, b1);
  EXPECT_EQ(b4->Size(), kBlockSize + kNextSize);

  Block* next = b4->NextAdjacentBlock();
  EXPECT_EQ(next, b3);

  EXPECT_THAT(FreelistList(), ElementsAre(b3->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFreeExact) {
  constexpr uint64_t kBlockSize = 0x500;
  constexpr uint64_t kNewSize = 0x800;
  constexpr uint64_t kNextSize = 0x300;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(kNextSize);
  AllocatedBlock* b3 = Alloc(0x220);
  Free(b2);

  AllocatedBlock* b4 = Realloc(b1, kNewSize);
  ASSERT_EQ(b4, b1);
  EXPECT_EQ(b4->Size(), kNewSize);

  EXPECT_THAT(FreelistList(), ElementsAre(b3->NextAdjacentBlock()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFreeTooLarge) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x700;
  constexpr uint64_t kNextSize = 0x200;

  AllocatedBlock* b1 = Alloc(kBlockSize);
  AllocatedBlock* b2 = Alloc(kNextSize);
  Alloc(0x150);
  Free(b2);

  AllocatedBlock* b4 = Realloc(b1, kNewSize);
  // b3 should have been placed elsewhere since b1 can't upsize in-place.
  EXPECT_NE(b4, b1);
  EXPECT_EQ(b4->Size(), kNewSize);

  EXPECT_THAT(
      FreelistList(),
      UnorderedElementsAre(b1->ToFree(), b4->NextAdjacentBlock()->ToFree()));
  EXPECT_THAT(ValidateHeap(), IsOk());
}

}  // namespace ckmalloc
