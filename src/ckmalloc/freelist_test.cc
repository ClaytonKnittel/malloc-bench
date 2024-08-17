#include "src/ckmalloc/freelist.h"

#include <cstdint>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

using testing::Address;
using testing::AnyOf;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using util::IsOk;

class FreelistTest : public ::testing::Test {
 public:
  Freelist& Freelist() {
    return freelist_;
  }

  LinkedList<TrackedBlock>& FreelistList() {
    return freelist_.free_blocks_;
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

  TrackedBlock* PushFree(size_t size) {
    CK_ASSERT_FALSE(phony_header_pushed_);
    CK_ASSERT_FALSE(last_free_);
    CK_ASSERT_TRUE(IsAligned<uint64_t>(size, kDefaultAlignment));
    CK_ASSERT_FALSE(Block::IsUntrackedSize(size));
    CK_ASSERT_LE(block_offset_ + size / sizeof(uint64_t), kRegionSize);

    TrackedBlock* block =
        Freelist()
            .InitFree(reinterpret_cast<Block*>(&region_[block_offset_]), size)
            ->ToTracked();
    block_offset_ += size / sizeof(uint64_t);
    last_free_ = true;
    return block;
  }

  AllocatedBlock* PushAllocated(size_t size) {
    CK_ASSERT_FALSE(phony_header_pushed_);
    CK_ASSERT_TRUE(IsAligned<uint64_t>(size, kDefaultAlignment));
    CK_ASSERT_GE(size, Block::kMinLargeSize);
    CK_ASSERT_LE(block_offset_ + size / sizeof(uint64_t), kRegionSize);

    AllocatedBlock* block = reinterpret_cast<Block*>(&region_[block_offset_])
                                ->InitAllocated(size, last_free_);
    block_offset_ += size / sizeof(uint64_t);
    last_free_ = false;
    return block;
  }

  UntrackedBlock* PushUntracked(size_t size) {
    CK_ASSERT_FALSE(phony_header_pushed_);
    CK_ASSERT_FALSE(last_free_);
    CK_ASSERT_TRUE(IsAligned<uint64_t>(size, kDefaultAlignment));
    CK_ASSERT_TRUE(Block::IsUntrackedSize(size));
    CK_ASSERT_LE(block_offset_ + size / sizeof(uint64_t), kRegionSize);

    UntrackedBlock* block =
        freelist_
            .InitFree(reinterpret_cast<Block*>(&region_[block_offset_]), size)
            ->ToUntracked();
    block_offset_ += size / sizeof(uint64_t);
    last_free_ = true;
    return block;
  }

  Block* PushPhony() {
    CK_ASSERT_LT(block_offset_, kRegionSize);

    Block* phony_block = reinterpret_cast<Block*>(&region_[block_offset_]);
    phony_block->InitPhonyHeader(last_free_);
    block_offset_++;
    phony_header_pushed_ = true;
    return phony_block;
  }

  // Validate invariants of the heap. This must be called after `PushPhony()`.
  absl::Status ValidateHeap();

 private:
  static constexpr size_t kRegionSize = 0x4000;

  class Freelist freelist_;
  size_t block_offset_ = 0;
  bool last_free_ = false;
  bool phony_header_pushed_ = false;
  uint64_t region_[kRegionSize] = {};
};

absl::Status FreelistTest::ValidateHeap() {
  if (!phony_header_pushed_) {
    return absl::FailedPreconditionError(
        "Called `ValidateHeap()` before `PushPhony()`");
  }

  std::vector<LargeSlabInfo> slabs = {
    {
        .start = &region_[0],
        .end = &region_[block_offset_],
        .slab = nullptr,
    },
  };
  return ValidateLargeSlabs(slabs, Freelist());
}

TEST_F(FreelistTest, FreeBlock) {
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

  EXPECT_THAT(FreelistList(), ElementsAre(Address(block)));
}

TEST_F(FreelistTest, UntrackedBlock) {
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

TEST_F(FreelistTest, PrevFree) {
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

TEST_F(FreelistTest, Empty) {
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(
      Freelist().FindFree(Block::UserSizeForBlockSize(Block::kMinLargeSize)),
      nullptr);
}

TEST_F(FreelistTest, OnlyAllocatedAndUntracked) {
  PushAllocated(0x140);
  PushAllocated(0x300);
  PushUntracked(0x80);
  PushAllocated(0x200);
  PushUntracked(0x10);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(
      Freelist().FindFree(Block::UserSizeForBlockSize(Block::kMinLargeSize)),
      nullptr);
}

TEST_F(FreelistTest, OneFree) {
  TrackedBlock* block = PushFree(0x100);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre(Address(block)));
  size_t max_req_size = Block::UserSizeForBlockSize(0x100);
  EXPECT_EQ(Freelist().FindFree(max_req_size), block);
  EXPECT_EQ(Freelist().FindFree(max_req_size + 1), nullptr);
}

TEST_F(FreelistTest, ManyFree) {
  PushAllocated(0x150);
  TrackedBlock* b1 = PushFree(0x500);
  PushAllocated(0x400);
  PushAllocated(0x160);
  TrackedBlock* b2 = PushFree(0x300);
  PushAllocated(0x240);
  TrackedBlock* b3 = PushFree(0x900);
  PushAllocated(0x150);
  TrackedBlock* b4 = PushFree(0x200);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), UnorderedElementsAre(Address(b1), Address(b2),
                                                   Address(b3), Address(b4)));

  size_t max_req_size = Block::UserSizeForBlockSize(0x900);
  EXPECT_EQ(Freelist().FindFree(max_req_size), b3);
  EXPECT_EQ(Freelist().FindFree(max_req_size + 1), nullptr);

  EXPECT_THAT(Freelist().FindFree(Block::UserSizeForBlockSize(0x500)),
              AnyOf(b1, b3));

  EXPECT_THAT(Freelist().FindFree(Block::UserSizeForBlockSize(0x300)),
              AnyOf(b1, b2, b3));

  EXPECT_THAT(Freelist().FindFree(Block::UserSizeForBlockSize(0x200)),
              AnyOf(b1, b2, b3, b4));
}

TEST_F(FreelistTest, FreeAsOnlyBlock) {
  constexpr uint64_t kBlockSize = 0x500;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushPhony();

  FreeBlock* free_block = Freelist().MarkFree(block);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(free_block->Size(), kBlockSize);

  EXPECT_THAT(FreelistList(), ElementsAre(Address(free_block)));
}

TEST_F(FreelistTest, FreeWithAllocatedNeighbors) {
  constexpr uint64_t kBlockSize = 0x500;

  PushAllocated(0x140);
  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushAllocated(0x180);
  PushPhony();

  FreeBlock* free_block = Freelist().MarkFree(block);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(free_block->Size(), kBlockSize);

  EXPECT_THAT(FreelistList(), ElementsAre(Address(free_block)));
}

TEST_F(FreelistTest, FreeWithFreePrev) {
  constexpr uint64_t kPrevSize = 0x240;
  constexpr uint64_t kBlockSize = 0x1100;

  TrackedBlock* b1 = PushFree(kPrevSize);
  AllocatedBlock* b2 = PushAllocated(kBlockSize);
  PushAllocated(0x800);
  PushPhony();
  EXPECT_THAT(FreelistList(), ElementsAre(Address(b1)));

  FreeBlock* free_block = Freelist().MarkFree(b2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(free_block, b1);
  EXPECT_EQ(free_block->Size(), kPrevSize + kBlockSize);

  EXPECT_THAT(FreelistList(), ElementsAre(Address(free_block)));
}

TEST_F(FreelistTest, FreeWithFreeNext) {
  constexpr uint64_t kBlockSize = 0x550;
  constexpr uint64_t kNextSize = 0x190;

  PushAllocated(0x600);
  AllocatedBlock* b1 = PushAllocated(kBlockSize);
  TrackedBlock* b2 = PushFree(kNextSize);
  PushPhony();
  EXPECT_THAT(FreelistList(), ElementsAre(Address(b2)));

  FreeBlock* free_block = Freelist().MarkFree(b1);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(free_block, static_cast<Block*>(b1));
  EXPECT_EQ(free_block->Size(), kBlockSize + kNextSize);

  EXPECT_THAT(FreelistList(), ElementsAre(Address(free_block)));
}

TEST_F(FreelistTest, FreeWithFreeNextAndPrev) {
  constexpr uint64_t kPrevSize = 0x150;
  constexpr uint64_t kBlockSize = 0x730;
  constexpr uint64_t kNextSize = 0x240;

  TrackedBlock* b1 = PushFree(kPrevSize);
  AllocatedBlock* b2 = PushAllocated(kBlockSize);
  TrackedBlock* b3 = PushFree(kNextSize);
  PushPhony();
  EXPECT_THAT(FreelistList(), UnorderedElementsAre(Address(b1), Address(b3)));

  FreeBlock* free_block = Freelist().MarkFree(b2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(free_block, b1);
  EXPECT_EQ(free_block->Size(), kPrevSize + kBlockSize + kNextSize);

  EXPECT_THAT(FreelistList(), ElementsAre(Address(free_block)));
}

TEST_F(FreelistTest, FreeWithUntrackedNeighbors) {
  constexpr uint64_t kPrevSize = 0x10;
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNextSize = 0x80;

  UntrackedBlock* b1 = PushUntracked(kPrevSize);
  AllocatedBlock* b2 = PushAllocated(kBlockSize);
  PushUntracked(kNextSize);
  PushPhony();
  EXPECT_THAT(FreelistList(), ElementsAre());

  FreeBlock* free_block = Freelist().MarkFree(b2);
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(free_block, static_cast<Block*>(b1));
  EXPECT_EQ(free_block->Size(), kPrevSize + kBlockSize + kNextSize);

  EXPECT_THAT(FreelistList(), ElementsAre(Address(free_block)));
}

TEST_F(FreelistTest, ResizeDown) {
  constexpr uint64_t kBlockSize = 0x530;
  constexpr uint64_t kNewSize = 0x340;

  PushAllocated(0x140);
  AllocatedBlock* b1 = PushAllocated(kBlockSize);
  AllocatedBlock* b2 = PushAllocated(0x190);
  PushPhony();

  ASSERT_TRUE(Freelist().ResizeIfPossible(b1, kNewSize));
  EXPECT_EQ(b1->Size(), kNewSize);

  Block* next = b1->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), b2);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(Address(next)));
}

TEST_F(FreelistTest, ResizeDownBeforeFree) {
  constexpr uint64_t kBlockSize = 0x290;
  constexpr uint64_t kNewSize = 0x130;
  constexpr uint64_t kNextSize = 0x140;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  Block* end_block = PushPhony();

  ASSERT_TRUE(Freelist().ResizeIfPossible(block, kNewSize));
  EXPECT_EQ(block->Size(), kNewSize);

  Block* next = block->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize + kNextSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), end_block);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(Address(next)));
}

TEST_F(FreelistTest, ResizeUpBeforeAllocated) {
  constexpr uint64_t kBlockSize = 0x290;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushAllocated(0x150);
  PushPhony();

  EXPECT_FALSE(
      Freelist().ResizeIfPossible(block, kBlockSize + kDefaultAlignment));
  EXPECT_EQ(block->Size(), kBlockSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre());
}

TEST_F(FreelistTest, ResizeUpBeforeFree) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x880;
  constexpr uint64_t kNextSize = 0x1100;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushFree(kNextSize);
  Block* end_block = PushPhony();

  ASSERT_TRUE(Freelist().ResizeIfPossible(block, kNewSize));
  EXPECT_EQ(block->Size(), kNewSize);

  Block* next = block->NextAdjacentBlock();
  EXPECT_EQ(next->Size(), kBlockSize + kNextSize - kNewSize);
  EXPECT_TRUE(next->Free());
  EXPECT_EQ(next->NextAdjacentBlock(), end_block);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(Address(next)));
}

TEST_F(FreelistTest, ResizeUpBeforeFreeTooLarge) {
  constexpr uint64_t kBlockSize = 0x490;
  constexpr uint64_t kNewSize = 0x600;
  constexpr uint64_t kNextSize = 0x90;

  AllocatedBlock* b1 = PushAllocated(kBlockSize);
  FreeBlock* b2 = PushFree(kNextSize);
  PushPhony();

  EXPECT_FALSE(Freelist().ResizeIfPossible(b1, kNewSize));
  EXPECT_EQ(b1->Size(), kBlockSize);

  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(FreelistList(), ElementsAre(Address(b2)));
}

}  // namespace ckmalloc
