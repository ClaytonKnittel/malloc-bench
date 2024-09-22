#pragma once

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/collections/intrusive_stack.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {

class FixedSizeFreeBlock {
 public:
  static FixedSizeFreeBlock* Init(void* ptr) {
    return new (ptr) FixedSizeFreeBlock();
  }

  class Stack : public IntrusiveStack<FixedSizeFreeBlock, Stack> {
   public:
    static constexpr Stack::Node* GetNode(FixedSizeFreeBlock* block) {
      return &block->node_;
    }
    static constexpr FixedSizeFreeBlock* GetItem(Node* node) {
      return twiddle::OwnerOf(node, &FixedSizeFreeBlock::node_);
    }
  };

 private:
  Stack::Node node_;
};

/**
 * Leases out fixed sized blocks within a memory region.
 */
template <size_t Size>
class FixedSizeFreeBlockAllocator {
 public:
  static constexpr size_t kSize = Size;

  /**
   * Returns an allocator that operates over the provided memory.
   */
  explicit FixedSizeFreeBlockAllocator(MemRegion& memory_region)
      : memory_region_(memory_region) {}

  /**
   * Returns a pointer to a free block of length `kSize`.
   */
  void* Allocate() {
    if (free_blocks_.empty()) {
      return memory_region_.Extend(kSize);
    }
    return free_blocks_.pop();
  }

  /**
   * Marks the block as free.
   */
  void Free(void* block) {
    free_blocks_.push(*FixedSizeFreeBlock::Init(block));
  }

 private:
  MemRegion& memory_region_;
  FixedSizeFreeBlock::Stack free_blocks_;
};

}  // namespace blocks
}  // namespace jsmalloc
