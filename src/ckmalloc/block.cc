#include "src/ckmalloc/block.h"

namespace ckmalloc {

constexpr size_t HeaderOffset() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
  return offsetof(FreeBlock, header_);
#pragma clang diagnostic pop
}

static_assert(HeaderOffset() == 0, "FreeBlock header offset must be 0");
static_assert(sizeof(FreeBlock) <= 24,
              "FreeBlock size is larger than 24 bytes");

uint64_t Block::Size() const {
  return header_ & kSizeMask;
}

void Block::SetSize(uint64_t size) {
  CK_ASSERT(size == (size & kSizeMask));
  header_ = size | (header_ & ~kSizeMask);
}

bool Block::Free() const {
  return (header_ & kFreeBitMask) != 0;
}

void Block::MarkFree(bool is_end_of_slab) {
  header_ |= kFreeBitMask;
  if (!is_end_of_slab) {
    uint64_t size = Size();
    Block* next = NextAdjacentBlock();
    // Write our footer at the end of this block.
    next->SetPrevFree(true);
    next->SetPrevSize(size);
  }
}

void Block::MarkAllocated() {
  header_ &= ~kFreeBitMask;
}

FreeBlock* Block::ToFree() {
  CK_ASSERT(Free());
  return static_cast<FreeBlock*>(this);
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
  CK_ASSERT(PrevFree());
  return *(&header_ - 1);
}

void Block::SetPrevSize(uint64_t size) {
  *(&header_ - 1) = size;
}

}  // namespace ckmalloc
