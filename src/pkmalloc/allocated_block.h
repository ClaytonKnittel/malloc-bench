#pragma once

#include "src/pkmalloc/block.h"
#include "src/pkmalloc/free_block.h"
#include "src/pkmalloc/global_state.h"

namespace pkmalloc {

class AllocatedBlock : public Block {
 public:
  // returns pointer to beginning of user information in a memory block
  uint8_t* GetBody();

  // changes type of void pointer to be uint8_t pointer
  static AllocatedBlock* FromRawPtr(void* ptr);

  // initializes free block to now be allocated and
  // returns pointer to beginning of this now allocated block
  AllocatedBlock* TakeFreeBlock();

  // extends the heap and initializes a new allocated block
  static AllocatedBlock* CreateBlockExtendHeap(size_t size,
                                               GlobalState* global_state);
  // this function shouldnt be in block **********

  // give the size needed for an allocated block to be 16 byte alligned and
  // including header size
  static size_t SpaceNeededWithHeader(const size_t& size);

  // changes the type of the current block from free to allocated
  static AllocatedBlock* FreeToAlloc(FreeBlock* current_block);

  // changes the type of the current block from allocated to free
  static FreeBlock* AllocToFree(AllocatedBlock* current_block);

  // still need header, keep 16 byte aligned so make header bits 8-16 of 16 byte
  // chunk*************

 private:
  // flexible size, returns pointer to beginning of body
  uint8_t body_[];
};

}  // namespace pkmalloc