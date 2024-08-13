#include "src/ckmalloc/freelist.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

using testing::Address;
using testing::AnyOf;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

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

 private:
  static constexpr size_t kRegionSize = 0x4000;

  class Freelist freelist_;
  size_t block_offset_ = 0;
  bool last_free_ = false;
  bool phony_header_pushed_ = false;
  uint64_t region_[kRegionSize] = {};
};

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

  // The freelist should remain empty with allocated or untracked blocks only.
  EXPECT_THAT(FreelistList(), ElementsAre());
  EXPECT_EQ(
      Freelist().FindFree(Block::UserSizeForBlockSize(Block::kMinLargeSize)),
      nullptr);
}

TEST_F(FreelistTest, OneFree) {
  FreeBlock* block = PushFree(0x100);
  PushPhony();

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

}  // namespace ckmalloc
