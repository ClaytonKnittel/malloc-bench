#include "block.h"

#include <cassert>
#include <cstddef>

#include "src/singleton_heap.h"

#define MALLOC_ASSERT(cond) assert(cond)
#define MALLOC_ASSERT_EQ(lhs, rhs)                                          \
  do {                                                                      \
    auto __l = (lhs);                                                       \
    auto __r = (rhs);                                                       \
    if (__l != __r) {                                                       \
      std::cerr << __FILE__ << ":" << __LINE__ << ": Expected equality of " \
                << std::hex << __l << " and " << __r << std::endl;          \
    }                                                                       \
  } while (0)

// #define MALLOC_ASSERT(cond)

// Block* take_free_block(size_t size) {
Block* Block::take_free_block() {
  // fix later, if you change size to be smaller, the remainder of this block
  // should be made to be a new free block SetBlockSize(size);
  Block::SetFree(false);
  return this;
}

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

void Block::SetBlockSize(uint64_t size) {
  // check first 4 bits are 0, 16 byte alligned size
  MALLOC_ASSERT((size & 0xf) == 0);
  MALLOC_ASSERT(size != 0);
  header_ = size | (header_ & 0x1);
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

uint8_t* Block::GetBody() {
  return body_;
}

void Block::CheckValid() const {
  MALLOC_ASSERT_EQ(magic_value_, 123456);
}

Block* Block::GetNextBlock() {
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(this) +
                                  GetBlockSize());
}

size_t Block::space_needed_with_header(const size_t& size) {
  // add size of header to size, 8 bytes
  size_t round_up = size + sizeof(Block);
  // round up size of memory needed to be 16 byte aligned
  round_up += 0xf;
  // zero out first four bits
  round_up = round_up & ~0xf;
  return round_up;
}

Block* Block::create_block_extend_heap(size_t size) {
  size_t block_size = Block::space_needed_with_header(size);
  auto* block = reinterpret_cast<Block*>(
      bench::SingletonHeap::GlobalInstance()->sbrk(block_size));
  block->SetBlockSize(block_size);
  block->SetFree(false);
  block->SetMagic();
  return block;
}