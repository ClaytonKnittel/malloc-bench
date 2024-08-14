#include "src/pkmalloc/allocated_block.h"

#include "src/pkmalloc/malloc_assert.h"

uint8_t* AllocatedBlock::GetBody() {
  return body_;
}

AllocatedBlock* AllocatedBlock::FromRawPtr(void* ptr) {
  return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(ptr) -
                                  offsetof(Block, body_));
}

// Block* take_free_block(size_t size) {
AllocatedBlock* AllocatedBlock::take_free_block() {
  // fix later, if you change size to be smaller, the remainder of this block
  // should be made to be a new free block SetBlockSize(size);
  Block::SetFree(false);
  return this;
}

void AllocatedBlock::SetBlockSize(uint64_t size) {
  // check first 4 bits are 0, 16 byte alligned size
  MALLOC_ASSERT((size & 0xf) == 0);
  MALLOC_ASSERT(size != 0);
  header_ = size | (header_ & 0x1);
}

AllocatedBlock* AllocatedBlock::create_block_extend_heap(size_t size) {
  size_t block_size = AllocatedBlock::space_needed_with_header(size);
  auto* block = reinterpret_cast<AllocatedBlock*>(
      bench::SingletonHeap::GlobalInstance()->sbrk(block_size));
  block->SetBlockSize(block_size);
  block->SetFree(false);
  block->SetMagic();
  return block;
}

size_t AllocatedBlock::space_needed_with_header(const size_t& size) {
  // add size of header to size, 8 bytes
  size_t round_up = size + sizeof(AllocatedBlock);
  // round up size of memory needed to be 16 byte aligned
  round_up += 0xf;
  // zero out first four bits
  round_up = round_up & ~0xf;
  return round_up;
}