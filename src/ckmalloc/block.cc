#include "src/ckmalloc/block.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

constexpr size_t HeaderOffset() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
  return offsetof(FreeBlock, header_);
#pragma clang diagnostic pop
}

constexpr size_t UserDataOffset() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
  return offsetof(AllocatedBlock, data_);
#pragma clang diagnostic pop
}

constexpr bool UserDataOffsetValid() {
  return UserDataOffset() == AllocatedBlock::kMetadataOverhead;
}

static_assert(HeaderOffset() == 0, "FreeBlock header offset must be 0");
static_assert(sizeof(FreeBlock) <= 24,
              "FreeBlock size is larger than 24 bytes");
static_assert(
    UserDataOffsetValid(),
    "User data region starts at the incorrect offset in allocated blocks.");
static_assert(sizeof(UntrackedBlock) + sizeof(uint64_t) <= Block::kMinBlockSize,
              "Untracked blocks + footers must be <= min block size");

AllocatedBlock* Block::InitAllocated(uint64_t size, bool prev_free) {
  CK_ASSERT_GE(size, kMinLargeSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  header_ = size | (prev_free ? kPrevFreeBitMask : 0);
  return ToAllocated();
}

UntrackedBlock* Block::InitUntracked(uint64_t size) {
  CK_ASSERT_GE(size, kMinBlockSize);
  CK_ASSERT_LT(size, kMinLargeSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  header_ = size | kFreeBitMask;
  WriteFooterAndPrevFree();
  return ToUntracked();
}

void Block::InitPhonyHeader(bool prev_free) {
  header_ = prev_free ? kPrevFreeBitMask : 0;
}

uint64_t Block::Size() const {
  return header_ & kSizeMask;
}

uint64_t Block::UserDataSize() const {
  CK_ASSERT_GE(Size(), kMinLargeSize);
  return UserSizeForBlockSize(Size());
}

bool Block::Free() const {
  return (header_ & kFreeBitMask) != 0;
}

bool Block::IsUntrackedSize() const {
  return IsUntrackedSize(Size());
}

FreeBlock* Block::ToFree() {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_GE(Size(), kMinLargeSize);
  return static_cast<FreeBlock*>(this);
}

const FreeBlock* Block::ToFree() const {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_GE(Size(), kMinLargeSize);
  return static_cast<const FreeBlock*>(this);
}

AllocatedBlock* Block::ToAllocated() {
  CK_ASSERT_FALSE(Free());
  CK_ASSERT_GE(Size(), kMinLargeSize);
  return static_cast<AllocatedBlock*>(this);
}

const AllocatedBlock* Block::ToAllocated() const {
  CK_ASSERT_FALSE(Free());
  CK_ASSERT_GE(Size(), kMinLargeSize);
  return static_cast<const AllocatedBlock*>(this);
}

UntrackedBlock* Block::ToUntracked() {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_LT(Size(), kMinLargeSize);
  return static_cast<UntrackedBlock*>(this);
}

const UntrackedBlock* Block::ToUntracked() const {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_LT(Size(), kMinLargeSize);
  return static_cast<const UntrackedBlock*>(this);
}

Block* Block::NextAdjacentBlock() {
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(this) + Size());
}

const Block* Block::NextAdjacentBlock() const {
  return reinterpret_cast<const Block*>(reinterpret_cast<const uint8_t*>(this) +
                                        Size());
}

Block* Block::PrevAdjacentBlock() {
  CK_ASSERT_TRUE(PrevFree());
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(this) -
                                  PrevSize());
}

const Block* Block::PrevAdjacentBlock() const {
  CK_ASSERT_TRUE(PrevFree());
  return reinterpret_cast<const Block*>(reinterpret_cast<const uint8_t*>(this) -
                                        PrevSize());
}

void Block::SetSize(uint64_t size) {
  CK_ASSERT_GE(size, kMinBlockSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  CK_ASSERT_EQ(size, (size & kSizeMask));
  header_ = size | (header_ & ~kSizeMask);
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

void* AllocatedBlock::UserDataPtr() {
  return data_;
}

/* static */
AllocatedBlock* AllocatedBlock::FromUserDataPtr(void* ptr) {
  return reinterpret_cast<AllocatedBlock*>(reinterpret_cast<uint8_t*>(ptr) -
                                           UserDataOffset());
}

}  // namespace ckmalloc
