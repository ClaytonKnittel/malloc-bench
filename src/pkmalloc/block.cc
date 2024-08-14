#include "block.h"

#include <cassert>
#include <cstddef>

#include "src/pkmalloc/malloc_assert.h"
#include "src/singleton_heap.h"

uint64_t Block::GetBlockSize() const {
  uint64_t block_size = header_ & ~0xf;
  CheckValid();
  MALLOC_ASSERT(block_size < bench::SingletonHeap::kHeapSize);
  MALLOC_ASSERT(block_size != 0);
  return block_size;
}

uint64_t Block::GetUserSize() const {
  return GetBlockSize() - 8;
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

void Block::SetMagic() {
  magic_value_ = 123456;
}

void Block::CheckValid() const {
  MALLOC_ASSERT_EQ(magic_value_, 123456);
}

Block* Block::GetNextBlock() {
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(this) +
                                  GetBlockSize());
}
