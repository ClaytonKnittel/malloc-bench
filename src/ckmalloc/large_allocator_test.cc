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
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

using testing::AnyOf;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using util::IsOk;

class LargeAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  LargeAllocatorTest()
      : heap_factory_(std::make_shared<TestHeapFactory>(kNumPages * kPageSize)),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(std::make_shared<SlabManagerFixture>(
            heap_factory_, slab_map_, /*heap_idx=*/0)),
        freelist_(std::make_shared<class Freelist>()),
        large_allocator_fixture_(std::make_shared<LargeAllocatorFixture>(
            heap_factory_, slab_map_, slab_manager_fixture_, freelist_)) {}

  TestHeapFactory& HeapFactory() {
    return slab_manager_fixture_->HeapFactory();
  }

  static bool PrevFree(const Block* block) {
    return block->PrevFree();
  }

  static void SetSize(Block* block, uint64_t size) {
    block->SetSize(size);
  }

  static void WriteFooterAndPrevFree(Block* block) {
    block->WriteFooterAndPrevFree();
  }

  bench::Heap& Heap() {
    return *HeapFactory().Instance(0);
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
    void* res =
        LargeAllocator().AllocLarge(Block::UserSizeForBlockSize(block_size));
    return res != nullptr ? AllocatedBlock::FromUserDataPtr(res) : nullptr;
  }

  AllocatedBlock* Realloc(AllocatedBlock* block, uint64_t block_size) {
    LargeSlab* slab =
        SlabMap().FindSlab(SlabManager().PageIdFromPtr(block))->ToLarge();
    void* res = LargeAllocator().ReallocLarge(
        slab, block->UserDataPtr(), Block::UserSizeForBlockSize(block_size));
    return res != nullptr ? AllocatedBlock::FromUserDataPtr(res) : nullptr;
  }

  void Free(AllocatedBlock* block) {
    LargeSlab* slab =
        SlabMap().FindSlab(SlabManager().PageIdFromPtr(block))->ToLarge();
    LargeAllocator().FreeLarge(slab, block->UserDataPtr());
  }

  Freelist& Freelist() {
    return large_allocator_fixture_->Freelist();
  }

  std::vector<const TrackedBlock*> FreelistList() {
    return large_allocator_fixture_->FreelistList();
  }

  absl::Status ValidateHeap() {
    CK_ASSERT_TRUE(PhonyHeaderPushed());
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() {
    CK_ASSERT_TRUE(PhonyHeaderPushed());
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateEmpty());
    return absl::OkStatus();
  }

  TrackedBlock* PushFree(size_t size) {
    CK_ASSERT_FALSE(PhonyHeaderPushed());
    CK_ASSERT_FALSE(LastFree());
    CK_ASSERT_TRUE(IsAligned<uint64_t>(size, kDefaultAlignment));
    CK_ASSERT_FALSE(Block::IsUntrackedSize(size));

    allocs_.emplace_back(size, true);
    TrackedBlock* block = static_cast<TrackedBlock*>(NextBlock());
    total_bytes_ += size;
    return block;
  }

  AllocatedBlock* PushAllocated(size_t size) {
    CK_ASSERT_FALSE(PhonyHeaderPushed());
    CK_ASSERT_TRUE(IsAligned<uint64_t>(size, kDefaultAlignment));
    CK_ASSERT_GE(size, Block::kMinTrackedSize);

    allocs_.emplace_back(size, false);
    AllocatedBlock* block = static_cast<AllocatedBlock*>(NextBlock());
    total_bytes_ += size;
    return block;
  }

  UntrackedBlock* PushUntracked(size_t size) {
    CK_ASSERT_FALSE(PhonyHeaderPushed());
    CK_ASSERT_FALSE(LastFree());
    CK_ASSERT_TRUE(IsAligned<uint64_t>(size, kDefaultAlignment));
    CK_ASSERT_TRUE(Block::IsUntrackedSize(size));

    allocs_.emplace_back(size, true);
    UntrackedBlock* block = static_cast<UntrackedBlock*>(NextBlock());
    total_bytes_ += size;
    return block;
  }

  Block* PushPhony() {
    // We may need to add an extra allocation at the end so the slab is
    // page-aligned.
    Block* filler_allocated_block = nullptr;

    const uint64_t total_used_bytes = Block::kFirstBlockInSlabOffset +
                                      total_bytes_ + Block::kMetadataOverhead;
    if (!IsAligned(total_used_bytes, kPageSize)) {
      uint64_t remainder =
          AlignUp(total_used_bytes, kPageSize) - total_used_bytes;
      if (remainder < Block::kMinBlockSize) {
        remainder += kPageSize;
      }
      allocs_.emplace_back(remainder, false);
      filler_allocated_block = NextBlock();
      total_bytes_ += remainder;
    }
    CK_ASSERT_TRUE(IsAligned(Block::kFirstBlockInSlabOffset + total_bytes_ +
                                 Block::kMetadataOverhead,
                             kPageSize));
    allocs_.emplace_back(0, false);

    Block* phony_block = NextBlock();
    phony_block->InitPhonyHeader(LastFree());

    auto result =
        SlabManager().Alloc<BlockedSlab>(CeilDiv(total_bytes_, kPageSize));
    CK_ASSERT_TRUE(result.has_value());
    CK_ASSERT_EQ(result.value().first, PageId::Zero());
    BlockedSlab* slab = result.value().second;

    uint64_t offset = Block::kFirstBlockInSlabOffset;
    bool prev_free = false;
    for (const auto& [size, is_free] : allocs_) {
      Block* block = PtrAdd<Block>(Heap().Start(), offset);
      if (size == 0) {
        break;
      }

      if (!is_free) {
        block->InitAllocated(size, prev_free);
        slab->AddAllocation(size);
      } else if (Block::IsUntrackedSize(size)) {
        Freelist().InitFree(block, size)->ToUntracked();
      } else {
        Freelist().InitFree(block, size)->ToTracked();
      }

      prev_free = is_free;
      offset += size;
    }

    return filler_allocated_block != nullptr ? filler_allocated_block
                                             : phony_block;
  }

 private:
  bool LastFree() {
    return !allocs_.empty() && allocs_.back().second;
  }

  bool PhonyHeaderPushed() {
    return !allocs_.empty() && allocs_.back().first == 0;
  }

  Block* NextBlock() {
    return PtrAdd<Block>(Heap().Start(),
                         Block::kFirstBlockInSlabOffset + total_bytes_);
  }

  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<class Freelist> freelist_;
  std::shared_ptr<LargeAllocatorFixture> large_allocator_fixture_;

  size_t total_bytes_ = 0;
  std::vector<std::pair<uint64_t, bool>> allocs_;
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
  Block block;

  Freelist().InitFree(&block, kBlockSize);
  EXPECT_TRUE(block.Free());
  EXPECT_EQ(block.Size(), kBlockSize);
  EXPECT_FALSE(PrevFree(&block));

  EXPECT_EQ(PtrDistance(block.NextAdjacentBlock(), &block), kBlockSize);

  EXPECT_TRUE(block.IsUntracked());

  // This should not cause assertion failure.
  block.ToUntracked();

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
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(Freelist().FindFree(Block::kMinTrackedSize), nullptr);
}

TEST_F(LargeAllocatorTest, OnlyAllocatedAndUntracked) {
  PushAllocated(0x140);
  PushAllocated(0x300);
  PushUntracked(0x80);
  PushAllocated(0x200);
  PushUntracked(0x30);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(Freelist().FindFree(Block::kMinTrackedSize), nullptr);
}

TEST_F(LargeAllocatorTest, OneFree) {
  constexpr uint64_t kSize = 0x110;

  TrackedBlock* block = PushFree(kSize);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre(block));
  EXPECT_EQ(Freelist().FindFree(kSize), block);
  EXPECT_EQ(Freelist().FindFree(kSize + kDefaultAlignment), nullptr);
}

TEST_F(LargeAllocatorTest, ManyFree) {
  PushAllocated(0x150);
  TrackedBlock* b1 = PushFree(0x500);
  PushAllocated(0x400);
  PushAllocated(0x160);
  TrackedBlock* b2 = PushFree(0x300);
  PushAllocated(0x240);
  TrackedBlock* b3 = PushFree(0x900);
  PushAllocated(0x150);
  TrackedBlock* b4 = PushFree(0x4B0);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), UnorderedElementsAre(b1, b2, b3, b4));

  EXPECT_EQ(Freelist().FindFree(0x900), b3);
  EXPECT_EQ(Freelist().FindFree(0x900 + kDefaultAlignment), nullptr);

  EXPECT_THAT(Freelist().FindFree(0x500), AnyOf(b1, b3));

  EXPECT_THAT(Freelist().FindFree(0x300), AnyOf(b1, b2, b3));

  EXPECT_THAT(Freelist().FindFree(0x200), AnyOf(b1, b2, b3, b4));
}

TEST_F(LargeAllocatorTest, Split) {
  constexpr uint64_t kBlockSize = 0xD30;
  constexpr uint64_t kNewBlockSize = 0x130;

  TrackedBlock* block = PushFree(kBlockSize);
  PushPhony();

  AllocatedBlock* alloc = Alloc(kNewBlockSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_EQ(static_cast<Block*>(alloc), static_cast<Block*>(block));
  EXPECT_EQ(alloc->Size(), kNewBlockSize);

  ASSERT_TRUE(alloc->NextAdjacentBlock()->Free());
  TrackedBlock* next_free = alloc->NextAdjacentBlock()->ToTracked();
  EXPECT_EQ(next_free->Size(), kBlockSize - kNewBlockSize);
  EXPECT_THAT(FreelistList(), ElementsAre(next_free));
}

TEST_F(LargeAllocatorTest, SplitWithMinBlockSizeRemainder) {
  constexpr uint64_t kBlockSize = 0xD30;
  constexpr uint64_t kNewBlockSize = 0xD10;
  static_assert(kBlockSize - kNewBlockSize == Block::kMinBlockSize);

  TrackedBlock* block = PushFree(kBlockSize);
  PushPhony();

  AllocatedBlock* alloc = Alloc(kNewBlockSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_EQ(static_cast<Block*>(alloc), static_cast<Block*>(block));
  EXPECT_EQ(alloc->Size(), kNewBlockSize);
}

TEST_F(LargeAllocatorTest, SplitWithBelowMinBlockSizeRemainder) {
  constexpr uint64_t kBlockSize = 0xD30;
  constexpr uint64_t kNewBlockSize = 0xD20;
  static_assert(kBlockSize - kNewBlockSize < Block::kMinBlockSize);

  TrackedBlock* block = PushFree(kBlockSize);
  AllocatedBlock* next_block = PushAllocated(0x2C0);
  PushPhony();

  AllocatedBlock* alloc = Alloc(kNewBlockSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  ASSERT_EQ(static_cast<Block*>(alloc), static_cast<Block*>(block));
  // The block should not be resized since it would leave a remaining free block
  // <= min block size.
  EXPECT_EQ(alloc->Size(), kBlockSize);
  EXPECT_EQ(alloc->NextAdjacentBlock(), next_block);
}

TEST_F(LargeAllocatorTest, FreeAsOnlyBlock) {
  constexpr uint64_t kBlockSize = 0xFF0;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushPhony();

  Free(block);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
}

TEST_F(LargeAllocatorTest, FreeWithAllocatedNeighbors) {
  constexpr uint64_t kBlockSize = 0xD30;

  PushAllocated(0x140);
  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushAllocated(0x180);
  PushPhony();

  Free(block);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(block->ToFree()));
}

TEST_F(LargeAllocatorTest, FreeWithFreePrev) {
  constexpr uint64_t kPrevSize = 0x240;
  constexpr uint64_t kBlockSize = 0x15B0;

  TrackedBlock* b1 = PushFree(kPrevSize);
  AllocatedBlock* b2 = PushAllocated(kBlockSize);
  PushPhony();
  EXPECT_THAT(FreelistList(), ElementsAre(b1));

  Free(b2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(b1->Size(), kPrevSize + kBlockSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(), ElementsAre(b1->ToFree()));
}

TEST_F(LargeAllocatorTest, FreeWithFreeNext) {
  constexpr uint64_t kBlockSize = 0x550;
  constexpr uint64_t kNextSize = 0x4A0;

  PushAllocated(0x600);
  AllocatedBlock* b1 = PushAllocated(kBlockSize);
  TrackedBlock* b2 = PushFree(kNextSize);
  PushPhony();
  EXPECT_THAT(FreelistList(), ElementsAre(b2));

  Free(b1);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(b1->Size(), kBlockSize + kNextSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(), ElementsAre(b1->ToFree()));
}

TEST_F(LargeAllocatorTest, FreeWithFreeNextAndPrev) {
  constexpr uint64_t kPrevSize = 0x150;
  constexpr uint64_t kBlockSize = 0x730;
  constexpr uint64_t kNextSize = 0x570;

  TrackedBlock* b1 = PushFree(kPrevSize);
  AllocatedBlock* b2 = PushAllocated(kBlockSize);
  TrackedBlock* b3 = PushFree(kNextSize);
  // Prevent the slab from being deallocated.
  PushAllocated(0x200);
  PushPhony();
  EXPECT_THAT(FreelistList(), UnorderedElementsAre(b1, b3));

  Free(b2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(b1->Size(), kPrevSize + kBlockSize + kNextSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(), ElementsAre(b1->ToFree()));
}

TEST_F(LargeAllocatorTest, FreeWithUntrackedNeighbors) {
  constexpr uint64_t kPrevSize = 0x30;
  constexpr uint64_t kBlockSize = 0x510;
  constexpr uint64_t kNextSize = 0x80;

  UntrackedBlock* b1 = PushUntracked(kPrevSize);
  AllocatedBlock* b2 = PushAllocated(kBlockSize);
  PushUntracked(kNextSize);
  // Prevent the slab from being deallocated.
  PushAllocated(0x200);
  PushPhony();
  EXPECT_THAT(FreelistList(), ElementsAre());

  Free(b2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(b1->Size(), kPrevSize + kBlockSize + kNextSize);

  ASSERT_TRUE(static_cast<Block*>(b1)->Free());
  EXPECT_THAT(FreelistList(), ElementsAre(b1->ToFree()));
}

TEST_F(LargeAllocatorTest, ResizeDown) {
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNewSize = 0x340;

  PushAllocated(0x140);
  AllocatedBlock* b1 = PushAllocated(kBlockSize);
  AllocatedBlock* b2 = PushAllocated(0x200);
  PushPhony();

  AllocatedBlock* b3 = Realloc(b1, kNewSize);
  ASSERT_EQ(b3, b1);
  EXPECT_EQ(b1, b3);
  EXPECT_EQ(b3->Size(), kNewSize);

  Block* next = b3->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), b2);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(next));
}

TEST_F(LargeAllocatorTest, ResizeDownBelowMinBlockSizeRemainder) {
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNewSize = 0x520;
  static_assert(kBlockSize - kNewSize < Block::kMinBlockSize);

  AllocatedBlock* b1 = PushAllocated(kBlockSize);
  AllocatedBlock* b2 = PushAllocated(0x200);
  PushPhony();

  AllocatedBlock* b3 = Realloc(b1, kNewSize);
  ASSERT_EQ(b3, b1);
  // The block can't change size since that would leave a remainder block < min
  // block size.
  EXPECT_EQ(b3->Size(), kBlockSize);

  Block* next = b3->NextAdjacentBlock();
  EXPECT_EQ(next, b2);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
}

TEST_F(LargeAllocatorTest, ResizeDownBeforeFree) {
  constexpr uint64_t kBlockSize = 0x290;
  constexpr uint64_t kNewSize = 0x130;
  constexpr uint64_t kNextSize = 0x140;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  Block* end_block = PushPhony();

  AllocatedBlock* b2 = Realloc(block, kNewSize);
  ASSERT_NE(b2, nullptr);
  EXPECT_EQ(b2, block);
  EXPECT_EQ(b2->Size(), kNewSize);

  Block* next = b2->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize + kNextSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), end_block);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(next));
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeAllocated) {
  constexpr uint64_t kBlockSize = 0x290;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushPhony();

  Block* b2 = Realloc(block, kBlockSize + kDefaultAlignment);
  // b2 should have been placed somewhere else since the block can't have
  // in-place upsized.
  ASSERT_NE(b2, block);
  EXPECT_EQ(block->Size(), kBlockSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(
      FreelistList(),
      UnorderedElementsAre(block->ToFree(), b2->NextAdjacentBlock()->ToFree()));
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFree) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x880;
  constexpr uint64_t kNextSize = 0x1100;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  Block* end_block = PushPhony();

  AllocatedBlock* b2 = Realloc(block, kNewSize);
  ASSERT_EQ(b2, block);
  EXPECT_EQ(b2->Size(), kNewSize);

  Block* next = b2->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize + kNextSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), end_block);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(next));
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFreeLessThanMinSizeRemainder) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x1580;
  constexpr uint64_t kNextSize = 0x1100;
  static_assert(kNewSize < kBlockSize + kNextSize);
  static_assert(kBlockSize + kNextSize - kNewSize < Block::kMinBlockSize);

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  Block* end_block = PushPhony();

  AllocatedBlock* b2 = Realloc(block, kNewSize);
  ASSERT_EQ(b2, block);
  EXPECT_EQ(b2->Size(), kBlockSize + kNextSize);

  Block* next = b2->NextAdjacentBlock();
  EXPECT_EQ(next, end_block);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFreeExact) {
  constexpr uint64_t kBlockSize = 0x500;
  constexpr uint64_t kNewSize = 0x800;
  constexpr uint64_t kNextSize = 0x300;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  PushPhony();

  AllocatedBlock* b2 = Realloc(block, kNewSize);
  ASSERT_EQ(b2, block);
  EXPECT_EQ(b2->Size(), kNewSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFreeTooLarge) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x600;
  constexpr uint64_t kNextSize = 0x90;

  AllocatedBlock* b1 = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  PushPhony();

  AllocatedBlock* b3 = Realloc(b1, kNewSize);
  // b3 should have been placed elsewhere since b1 can't upsize in-place.
  EXPECT_NE(b3, b1);
  EXPECT_EQ(b3->Size(), kNewSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(),
              ElementsAre(b1->ToFree(), b3->NextAdjacentBlock()->ToFree()));
}

}  // namespace ckmalloc
