#pragma once

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

/**
 * An allocator of free blocks.
 */
class FreeBlockAllocator {
 public:
  /**
   * Returns a FreeBlock allocator that depends
   * on the provided heap-based allocator.
   */
  explicit FreeBlockAllocator(Allocator& allocator);

  /**
   * Returns a pointer to some free space of exactly the given size.
   */
  FreeBlock* Allocate(size_t size);

  /**
   * Marks the block as free.
   */
  void Free(BlockHeader* block);

 private:
  Allocator& allocator_;
  FreeBlock::List free_blocks_;
};

/**
 * Adapts FreeBlockAllocator so it can be used where an Allocator is needed.
 */
class FreeBlockAllocatorAdaptor : public Allocator {
 public:
  explicit FreeBlockAllocatorAdaptor(FreeBlockAllocator& allocator)
      : allocator_(allocator){};

  void* Allocate(size_t size) override {
    return static_cast<void*>(allocator_.Allocate(size));
  }

  void Free(void* ptr) override {
    allocator_.Free(static_cast<BlockHeader*>(ptr));
  }

 private:
  FreeBlockAllocator& allocator_;
};

}  // namespace blocks
}  // namespace jsmalloc
