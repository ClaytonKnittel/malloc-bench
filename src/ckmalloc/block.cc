#include "src/ckmalloc/block.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

constexpr size_t HeaderOffset() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
  static_assert(offsetof(ExactSizeBlock, header_) ==
                offsetof(TreeBlock, header_));
  return offsetof(ExactSizeBlock, header_);
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
static_assert(
    UserDataOffsetValid(),
    "User data region starts at the incorrect offset in allocated blocks.");
static_assert(sizeof(UntrackedBlock) + sizeof(uint64_t) <= Block::kMinBlockSize,
              "Untracked blocks + footers must be <= default alignment");

AllocatedBlock* Block::InitAllocated(uint64_t size, bool prev_free) {
  CK_ASSERT_GE(size, kMinBlockSize);
  CK_ASSERT_TRUE(IsAligned(size, kDefaultAlignment));
  header_ = size | (prev_free ? kPrevFreeBitMask : 0);
  return ToAllocated();
}

void Block::InitPhonyHeader(bool prev_free) {
  header_ = prev_free ? kPrevFreeBitMask : 0;
}

uint64_t Block::Size() const {
  return header_ & kSizeMask;
}

uint64_t Block::UserDataSize() const {
  CK_ASSERT_GE(Size(), kMinBlockSize);
  return UserSizeForBlockSize(Size());
}

bool Block::Free() const {
  return (header_ & kFreeBitMask) != 0;
}

bool Block::IsUntracked() const {
  return IsUntrackedSize(Size());
}

bool Block::IsTracked() const {
  return !IsUntracked();
}

bool Block::IsExactSize() const {
  return !IsUntrackedSize(Size()) && Size() <= kMaxExactSizeBlock;
}

bool Block::IsPhonyHeader() const {
  return Size() == 0;
}

AllocatedBlock* Block::ToAllocated() {
  CK_ASSERT_FALSE(Free());
  return static_cast<AllocatedBlock*>(this);
}

const AllocatedBlock* Block::ToAllocated() const {
  CK_ASSERT_FALSE(Free());
  return static_cast<const AllocatedBlock*>(this);
}

FreeBlock* Block::ToFree() {
  CK_ASSERT_TRUE(Free());
  return static_cast<FreeBlock*>(this);
}

const FreeBlock* Block::ToFree() const {
  CK_ASSERT_TRUE(Free());
  return static_cast<const FreeBlock*>(this);
}

TrackedBlock* Block::ToTracked() {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_GT(Size(), kMaxUntrackedSize);
  return static_cast<TrackedBlock*>(this);
}

const TrackedBlock* Block::ToTracked() const {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_GT(Size(), kMaxUntrackedSize);
  return static_cast<const TrackedBlock*>(this);
}

ExactSizeBlock* Block::ToExactSize() {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_LE(Size(), kMaxExactSizeBlock);
  return static_cast<ExactSizeBlock*>(this);
}

const ExactSizeBlock* Block::ToExactSize() const {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_LE(Size(), kMaxExactSizeBlock);
  return static_cast<const ExactSizeBlock*>(this);
}

TreeBlock* Block::ToTree() {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_GT(Size(), kMaxExactSizeBlock);
  return static_cast<TreeBlock*>(this);
}

const TreeBlock* Block::ToTree() const {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_GT(Size(), kMaxExactSizeBlock);
  return static_cast<const TreeBlock*>(this);
}

UntrackedBlock* Block::ToUntracked() {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_LE(Size(), kMaxUntrackedSize);
  return static_cast<UntrackedBlock*>(this);
}

const UntrackedBlock* Block::ToUntracked() const {
  CK_ASSERT_TRUE(Free());
  CK_ASSERT_LE(Size(), kMaxUntrackedSize);
  return static_cast<const UntrackedBlock*>(this);
}

Block* Block::NextAdjacentBlock() {
  return PtrAdd<Block>(this, Size());
}

const Block* Block::NextAdjacentBlock() const {
  return PtrAdd<const Block>(this, Size());
}

Block* Block::PrevAdjacentBlock() {
  CK_ASSERT_TRUE(PrevFree());
  return PtrSub<Block>(this, PrevSize());
}

const Block* Block::PrevAdjacentBlock() const {
  CK_ASSERT_TRUE(PrevFree());
  return PtrSub<const Block>(this, PrevSize());
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

Void* AllocatedBlock::UserDataPtr() {
  return data_;
}

const Void* AllocatedBlock::UserDataPtr() const {
  return data_;
}

/* static */
AllocatedBlock* AllocatedBlock::FromUserDataPtr(Void* ptr) {
  return PtrSub<AllocatedBlock>(ptr, UserDataOffset());
}

}  // namespace ckmalloc
