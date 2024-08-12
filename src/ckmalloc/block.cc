#include "src/ckmalloc/block.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

constexpr size_t HeaderOffset() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
  return offsetof(FreeBlock, header_);
#pragma clang diagnostic pop
}

constexpr bool UserDataOffsetValid() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
  return offsetof(AllocatedBlock, data_) == AllocatedBlock::kMetadataOverhead;
#pragma clang diagnostic pop
}

static_assert(HeaderOffset() == 0, "FreeBlock header offset must be 0");
static_assert(sizeof(FreeBlock) <= 24,
              "FreeBlock size is larger than 24 bytes");
static_assert(
    UserDataOffsetValid(),
    "User data region starts at the incorrect offset in allocated blocks.");

/* static */
uint64_t Block::BlockSizeForUserSize(size_t user_size) {
  return std::max(AlignUp(user_size + kMetadataOverhead, kDefaultAlignment),
                  kMinBlockSize);
}

FreeBlock* Block::InitFree(uint64_t size,
                           LinkedList<FreeBlock>& free_block_list,
                           bool is_end_of_slab) {
  CK_ASSERT_GE(size, kMinBlockSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  // Prev free is never true for free blocks, so we will not set that.
  header_ = size | kFreeBitMask;
  if (!is_end_of_slab) {
    WriteFooterAndPrevFree();
  }

  FreeBlock* free_block = ToFree();
  free_block_list.InsertFront(free_block);
  return free_block;
}

AllocatedBlock* Block::InitAllocated(uint64_t size, bool prev_free) {
  CK_ASSERT_GE(size, kMinBlockSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  header_ = size | (prev_free ? kPrevFreeBitMask : 0);
  return ToAllocated();
}

uint64_t Block::Size() const {
  return header_ & kSizeMask;
}

void Block::SetSize(uint64_t size) {
  CK_ASSERT_GE(size, kMinBlockSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  CK_ASSERT_EQ(size, (size & kSizeMask));
  header_ = size | (header_ & ~kSizeMask);
}

uint64_t Block::UserDataSize() const {
  return Size() - kMetadataOverhead;
}

bool Block::Free() const {
  return (header_ & kFreeBitMask) != 0;
}

FreeBlock* Block::ToFree() {
  CK_ASSERT_TRUE(Free());
  return static_cast<FreeBlock*>(this);
}

AllocatedBlock* Block::ToAllocated() {
  CK_ASSERT_FALSE(Free());
  return static_cast<AllocatedBlock*>(this);
}

Block* Block::NextAdjacentBlock() {
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(this) + Size());
}

const Block* Block::NextAdjacentBlock() const {
  return reinterpret_cast<const Block*>(reinterpret_cast<const uint8_t*>(this) +
                                        Size());
}

Block* Block::PrevAdjacentBlock() {
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(this) -
                                  PrevSize());
}

const Block* Block::PrevAdjacentBlock() const {
  return reinterpret_cast<const Block*>(reinterpret_cast<const uint8_t*>(this) -
                                        PrevSize());
}

bool Block::PrevFree() const {
  return (header_ & kPrevFreeBitMask) != 0;
}

void Block::SetPrevFree(bool free) {
  if (free) {
    header_ |= kPrevFreeBitMask;
  } else {
    header_ &= ~kPrevFreeBitMask;
  }
}

uint64_t Block::PrevSize() const {
  CK_ASSERT_TRUE(PrevFree());
  return *(&header_ - 1);
}

void Block::SetPrevSize(uint64_t size) {
  *(&header_ - 1) = size;
}

void Block::WriteFooterAndPrevFree() {
  uint64_t size = Size();
  Block* next = NextAdjacentBlock();
  // Write our footer at the end of this block.
  next->SetPrevFree(true);
  next->SetPrevSize(size);
}

AllocatedBlock* FreeBlock::MarkAllocated() {
  // Remove ourselves from the freelist we are in.
  LinkedListNode::Remove();

  // Clear the free bit.
  header_ &= ~kFreeBitMask;
  return ToAllocated();
}

std::pair<AllocatedBlock*, FreeBlock*> FreeBlock::Split(
    uint64_t block_size, LinkedList<FreeBlock>& free_block_list) {
  uint64_t size = Size();
  CK_ASSERT_LE(block_size, size);

  uint64_t remainder = size - block_size;
  if (remainder < kMinBlockSize) {
    AllocatedBlock* block = MarkAllocated();
    return std::make_pair(block, nullptr);
  }

  SetSize(block_size);
  AllocatedBlock* block = MarkAllocated();

  Block* remainder_block = block->NextAdjacentBlock();
  FreeBlock* remainder_free_block =
      remainder_block->InitFree(remainder, free_block_list);

  return std::make_pair(block, remainder_free_block);
}

void* AllocatedBlock::UserDataPtr() {
  return data_;
}

FreeBlock* AllocatedBlock::MarkFree(LinkedList<FreeBlock>& free_block_list,
                                    bool is_end_of_slab) {
  header_ |= kFreeBitMask;
  if (!is_end_of_slab) {
    WriteFooterAndPrevFree();
  }

  FreeBlock* free_block = ToFree();
  free_block_list.InsertFront(free_block);
  return free_block;
}

}  // namespace ckmalloc
