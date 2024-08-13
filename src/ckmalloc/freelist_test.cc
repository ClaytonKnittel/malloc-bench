#include "src/ckmalloc/freelist.h"

#include <cstdint>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/testlib.h"  // IWYU pragma: keep
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

  LinkedList<FreeBlock>& FreelistList() {
    return freelist_.free_blocks_;
  }

  static bool PrevFree(const Block* block) {
    return block->PrevFree();
  }

  static void SetSize(Block* block, uint64_t size) {
    block->SetSize(size);
  }

  FreeBlock* PushFree(size_t size) {
    CK_ASSERT_FALSE(phony_header_pushed_);
    CK_ASSERT_FALSE(last_free_);
    CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
    CK_ASSERT_GE(size, Block::kMinLargeSize);
    CK_ASSERT_LE(block_offset_ + size / sizeof(uint64_t), kRegionSize);

    FreeBlock* block = Freelist().InitFree(
        reinterpret_cast<Block*>(&region_[block_offset_]), size);
    block_offset_ += size / sizeof(uint64_t);
    last_free_ = true;
    return block;
  }

  AllocatedBlock* PushAllocated(size_t size) {
    CK_ASSERT_FALSE(phony_header_pushed_);
    CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
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
    CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
    CK_ASSERT_LT(size, Block::kMinLargeSize);
    CK_ASSERT_LE(block_offset_ + size / sizeof(uint64_t), kRegionSize);

    UntrackedBlock* block =
        reinterpret_cast<Block*>(&region_[block_offset_])->InitUntracked(size);
    block_offset_ += size / sizeof(uint64_t);
    last_free_ = true;
    return block;
  }

  void PushPhony() {
    CK_ASSERT_LT(block_offset_, kRegionSize);

    reinterpret_cast<Block*>(&region_[block_offset_])
        ->InitPhonyHeader(last_free_);
    block_offset_++;
    phony_header_pushed_ = true;
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

  absl::flat_hash_set<const Block*> free_blocks;
  void* const heap_start = &region_[0];
  void* const heap_end = &region_[block_offset_];
  // Iterate over the freelist.
  for (const FreeBlock& block : FreelistList()) {
    if (&block < heap_start || &block >= heap_end) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered block outside the range of the heap in "
                          "freelist: block at %p, heap ranges from %p to %p",
                          &block, heap_start, heap_end));
    }

    size_t block_offset_bytes = PtrDistance(&block, heap_start);
    if (!IsAligned(block_offset_bytes, kDefaultAlignment)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered unaligned block in freelist at offset "
                          "%zu from heap start: %v",
                          block_offset_bytes, block));
    }

    if (!static_cast<const Block*>(&block)->Free()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered non-free block in freelist: %v", block));
    }

    auto [it, inserted] = free_blocks.insert(&block);
    if (!inserted) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Detected loop in freelist at block %v", block));
    }
  }

  // Iterate over the heap.
  Block* block = reinterpret_cast<Block*>(&region_[0]);
  Block* prev_block = nullptr;
  size_t n_free_blocks = 0;
  while (block->Size() != 0) {
    if (block < heap_start || block >= heap_end) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Encountered block outside the range of the heap while iterating "
          "over heap: block at %p, heap ranges from %p to %p",
          &block, heap_start, heap_end));
    }

    size_t block_offset_bytes = PtrDistance(block, heap_start);
    if (!IsAligned(block_offset_bytes, kDefaultAlignment)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered unaligned block while iterating heap at "
                          "offset %zu from heap start: %v",
                          block_offset_bytes, *block));
    }

    size_t block_offset = reinterpret_cast<uint64_t*>(block) - &region_[0];
    if (block_offset >= block_offset_) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered block out of range of the heap at "
                          "address %p, heap ranges from %p to %p",
                          block, &region_[0], &region_[block_offset_]));
    }

    if (block->Free()) {
      bool in_freelist = free_blocks.contains(block);
      if (block->IsUntrackedSize() && in_freelist) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Encountered untracked block in the freelist: %v", *block));
      }
      if (!block->IsUntrackedSize()) {
        if (!in_freelist) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Encountered free block which was not in freelist: %v", *block));
        }
        n_free_blocks++;
      }

      if (prev_block != nullptr && prev_block->Free()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Encountered two free blocks in a row: %v and %v",
                            *prev_block, *block));
      }
    } else if (block->Size() < Block::kMinLargeSize) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Encountered small-sized allocated block, which "
                          "should not be possible: %v",
                          *block));
    }

    if (prev_block != nullptr && prev_block->Free()) {
      if (!PrevFree(block)) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Prev-free bit not set in block after free block: "
                            "%v followed by %v",
                            *prev_block, *block));
      }
      if (block->PrevSize() != prev_block->Size()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Prev-size incorrect for block after free block: %v followed by %v",
            *prev_block, *block));
      }
    }

    if (block->Size() > (block_offset_ - block_offset) * sizeof(uint64_t)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Encountered block with size larger than remainder of heap: %v, heap "
          "has %zu bytes left",
          *block, (block_offset_ - block_offset) * sizeof(uint64_t)));
    }

    prev_block = block;
    block = block->NextAdjacentBlock();
  }

  if (block != reinterpret_cast<Block*>(&region_[block_offset_ - 1])) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Ended heap iteration on block not at end of heap: %p, "
                        "end of heap is %p",
                        block, &region_[block_offset_ - 1]));
  }

  if (n_free_blocks != free_blocks.size()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Encountered %zu free blocks when iterating over the "
                        "heap, but %zu free blocks in the freelist",
                        n_free_blocks, free_blocks.size()));
  }

  return absl::OkStatus();
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
  EXPECT_FALSE(block->IsUntrackedSize());

  // This should not trigger an assertion failure.
  block->ToFree();
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
  PushAllocated(0x100);
  PushAllocated(0x300);
  PushUntracked(0x10);
  PushAllocated(0x20);
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
  FreeBlock* block = PushFree(0x100);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre(Address(block)));
  size_t max_req_size = Block::UserSizeForBlockSize(0x100);
  EXPECT_EQ(Freelist().FindFree(max_req_size), block);
  EXPECT_EQ(Freelist().FindFree(max_req_size + 1), nullptr);
}

TEST_F(FreelistTest, ManyFree) {
  PushAllocated(0x100);
  FreeBlock* b1 = PushFree(0x500);
  PushAllocated(0x400);
  PushAllocated(0x50);
  FreeBlock* b2 = PushFree(0x300);
  PushAllocated(0x30);
  FreeBlock* b3 = PushFree(0x900);
  PushAllocated(0x50);
  FreeBlock* b4 = PushFree(0x200);
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

TEST_F(FreelistTest, FreeWithoutCoalesce) {
  constexpr uint64_t kBlockSize = 0x500;

  AllocatedBlock* block = PushAllocated(kBlockSize);
  PushPhony();
  EXPECT_THAT(ValidateHeap(), IsOk());

  FreeBlock* free_block = Freelist().MarkFree(block);
  EXPECT_EQ(free_block->Size(), kBlockSize);

  EXPECT_THAT(FreelistList(), ElementsAre(Address(free_block)));
}

}  // namespace ckmalloc
