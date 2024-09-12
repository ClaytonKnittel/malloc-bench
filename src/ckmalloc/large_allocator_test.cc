#include <cstddef>
#include <cstdint>
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
        large_allocator_fixture_(std::make_shared<LargeAllocatorFixture>(
            heap_factory_, slab_map_, slab_manager_fixture_)) {}

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

  AllocatedBlock* Realloc(AllocatedBlock* block, size_t user_size) {
    LargeSlab* slab =
        SlabMap().FindSlab(SlabManager().PageIdFromPtr(block))->ToLarge();
    void* res =
        LargeAllocator().ReallocLarge(slab, block->UserDataPtr(), user_size);
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
    CK_ASSERT_GE(size, Block::kMinLargeSize);

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
    if (!IsAligned(Block::kFirstBlockInSlabOffset + total_bytes_ +
                       Block::kMetadataOverhead,
                   kPageSize)) {
      CK_ASSERT_EQ(Block::kFirstBlockInSlabOffset + total_bytes_ +
                       Block::kMetadataOverhead,
                   kPageSize);
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

    return phony_block;
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
  EXPECT_FALSE(block->IsUntracked());

  // This should not trigger an assertion failure.
  block->ToTracked();
}

TEST_F(LargeAllocatorTest, UntrackedBlock) {
  constexpr size_t kBlockSize = 0x10;
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

  // First initialize the block as untracked, which is a type of free block,
  // then artificially increase its size to the desired size. This has to be
  // done because we can't directly initialize a free block (this requires
  // insertion into a freelist), and `InitUntracked` checks that the size is
  // below the min large block size.
  Freelist().InitFree(prev, 0x10)->ToUntracked();
  SetSize(prev, kBlockSize);

  // This should write to the footer of `block`, and set block's prev_free bit.
  WriteFooterAndPrevFree(prev);

  EXPECT_TRUE(PrevFree(block));
  EXPECT_EQ(block->PrevAdjacentBlock(), prev);
}

TEST_F(LargeAllocatorTest, Empty) {
  PushAllocated(0xff0);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(Freelist().FindFree(Block::kMinLargeSize), nullptr);
}

TEST_F(LargeAllocatorTest, OnlyAllocatedAndUntracked) {
  PushAllocated(0x140);
  PushAllocated(0x300);
  PushUntracked(0x80);
  PushAllocated(0x200);
  PushUntracked(0x10);
  PushAllocated(0x920);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(Freelist().FindFree(Block::kMinLargeSize), nullptr);
}

TEST_F(LargeAllocatorTest, OneFree) {
  uint64_t kSize = 0x100;

  TrackedBlock* block = PushFree(kSize);
  PushAllocated(0xEF0);
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
  PushAllocated(0x800);
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
  constexpr uint64_t kPrevSize = 0x10;
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNextSize = 0x80;

  UntrackedBlock* b1 = PushUntracked(kPrevSize);
  AllocatedBlock* b2 = PushAllocated(kBlockSize);
  PushUntracked(kNextSize);
  // Prevent the slab from being deallocated.
  PushAllocated(0xA30);
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
  AllocatedBlock* b2 = PushAllocated(0x980);
  PushPhony();

  AllocatedBlock* b3 = Realloc(b1, Block::UserSizeForBlockSize(kNewSize));
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

TEST_F(LargeAllocatorTest, ResizeDownBeforeFree) {
  constexpr uint64_t kBlockSize = 0x290;
  constexpr uint64_t kNewSize = 0x130;
  constexpr uint64_t kNextSize = 0x140;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  Block* end_block = PushAllocated(0xC20);
  PushPhony();

  AllocatedBlock* b2 = Realloc(block, Block::UserSizeForBlockSize(kNewSize));
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
  PushAllocated(0xD60);
  PushPhony();

  Block* b2 = Realloc(block, Block::UserSizeForBlockSize(kBlockSize) + 1);
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
  Block* end_block = PushAllocated(0xA60);
  PushPhony();

  AllocatedBlock* b2 = Realloc(block, Block::UserSizeForBlockSize(kNewSize));
  ASSERT_EQ(b2, block);
  EXPECT_EQ(b2->Size(), kNewSize);

  Block* next = b2->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize + kNextSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), end_block);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(next));
}

TEST_F(LargeAllocatorTest, ResizeUpBeforeFreeExact) {
  constexpr uint64_t kBlockSize = 0x500;
  constexpr uint64_t kNewSize = 0x800;
  constexpr uint64_t kNextSize = 0x300;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  PushAllocated(0x7F0);
  PushPhony();

  AllocatedBlock* b2 = Realloc(block, Block::UserSizeForBlockSize(kNewSize));
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
  PushAllocated(0xAD0);
  PushPhony();

  AllocatedBlock* b3 = Realloc(b1, Block::UserSizeForBlockSize(kNewSize));
  // b3 should have been placed elsewhere since b1 can't upsize in-place.
  EXPECT_NE(b3, b1);
  EXPECT_EQ(b3->Size(), kNewSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(),
              ElementsAre(b1->ToFree(), b3->NextAdjacentBlock()->ToFree()));
}

}  // namespace ckmalloc