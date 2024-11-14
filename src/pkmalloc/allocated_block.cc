#include "src/pkmalloc/allocated_block.h"

#include <cstddef>

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/pkmalloc/global_state.h"

namespace pkmalloc {

uint8_t* AllocatedBlock::GetBody() {
  return body_;
}

AllocatedBlock* AllocatedBlock::FromRawPtr(void* ptr) {
  return reinterpret_cast<AllocatedBlock*>(reinterpret_cast<uint8_t*>(ptr) -
                                           offsetof(AllocatedBlock, body_));
}

AllocatedBlock* AllocatedBlock::TakeFreeBlock() {
  // fix later, if you change size to be smaller, the remainder of this block
  // should be made to be a new free block SetBlockSize(size);
  // should take parameter size_t size in the future to allow user to change
  // size of allocated block
  Block::SetFree(false);
  return this;
}

AllocatedBlock* AllocatedBlock::CreateBlockExtendHeap(
    size_t size, GlobalState* global_state) {
  size_t block_size = AllocatedBlock::SpaceNeededWithHeader(size);
  bench::Heap* heap = global_state->GetGlobalHeapStart(global_state);
  AllocatedBlock* new_allocated_block =
      static_cast<AllocatedBlock*>(heap->sbrk(block_size));
  new_allocated_block->SetBlockSize(block_size);
  new_allocated_block->SetFree(false);
  return new_allocated_block;
}

size_t AllocatedBlock::SpaceNeededWithHeader(const size_t& size) {
  // add size of header to size, 8 bytes
  size_t round_up = size + sizeof(AllocatedBlock);
  // round up size of memory, needs to be 16 byte aligned
  round_up += 0xf;
  // zero out first four bits
  round_up = round_up & ~0xf;
  return round_up;
}

AllocatedBlock* AllocatedBlock::FreeToAlloc(FreeBlock* current_block) {
  current_block->SetFree(false);
  auto* result = reinterpret_cast<AllocatedBlock*>(current_block);
  return result;
}

FreeBlock* AllocatedBlock::AllocToFree(AllocatedBlock* current_block) {
  current_block->SetFree(true);
  auto* result = reinterpret_cast<FreeBlock*>(current_block);
  return result;
}

}  // namespace pkmalloc