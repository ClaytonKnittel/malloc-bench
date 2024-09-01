#include "block.h"

#include <cassert>

#include "src/pkmalloc/malloc_assert.h"
#include "src/singleton_heap.h"

uint64_t Block::GetBlockSize() const {
  uint64_t block_size = header_ & ~0xf;
  MALLOC_ASSERT(block_size < bench::SingletonHeap::kHeapSize);
  MALLOC_ASSERT(block_size != 0);
  return block_size;
}

uint64_t Block::GetUserSize() const {
  return GetBlockSize() - sizeof(header_);
}

bool Block::IsFree() const {
  return (header_ & 0x1) == 0x1;
}

void Block::SetFree(bool free) {
  if (free) {
    header_ = header_ | 0x1;
  } else {
    header_ = header_ & ~0x1;
  }
}

Block* Block::GetNextBlock() {
  // ************ needs to make sure current block isnt end of heap ***********
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(this) +
                                  GetBlockSize());
}

void Block::SetBlockSize(uint64_t size) {
  // check first 4 bits are 0, 16 byte alligned size
  MALLOC_ASSERT((size & 0xf) == 0);
  MALLOC_ASSERT(size != 0);
  header_ = size | (header_ & 0x1);
}
