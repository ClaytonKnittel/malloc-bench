#pragma once

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/collections/intrusive_linked_list.h"
#include "src/jsmalloc/collections/rbtree.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {

class FreeBlock;
struct FreeBlockFooter {
  FreeBlock* block;
};

class FreeBlockList;

/**
 * A free block.
 */
class FreeBlock {
 public:
  /** Creates a new free block. */
  static FreeBlock* New(SentinelBlockHeap& heap, size_t size);

  /** Marks the provided block as free. */
  static FreeBlock* MarkFree(BlockHeader* block);

  /**
   * Marks this block as used.
   *
   * Resizes this block and returns a newly sharded `FreeBlock`, if created.
   * Returns nullptr if no new free block was created.
   */
  FreeBlock* MarkUsed(size_t new_block_size);

  /**
   * Marks this block as used.
   */
  FreeBlock* MarkUsed();

  /**
   * Whether `MarkUsed` can be called with a resize.
   */
  bool CanMarkUsed(size_t new_block_size) const;

  /**
   * The size of this free block.
   */
  size_t BlockSize() const;

  BlockHeader* Header();

  /** Consumes the next block. */
  FreeBlock* NextBlockIfFree();

  /** Returns the previous block, if free. */
  FreeBlock* PrevBlockIfFree();

  /** Consumes the next block. */
  void ConsumeNextBlock();

  class TreeComparator {
   public:
    bool operator()(const FreeBlock& lhs, const FreeBlock& rhs) const {
      return lhs.BlockSize() < rhs.BlockSize();
    }
  };

  class Tree : public RbTree<FreeBlock, Tree, TreeComparator> {
   public:
    class Comparator {
      bool operator()(const FreeBlock& lhs, const FreeBlock& rhs) const {
        return lhs.BlockSize() < rhs.BlockSize();
      }
    };

    static constexpr RbNode* GetNode(FreeBlock* block) {
      return &block->free_tree_node_;
    }

    static constexpr FreeBlock* GetItem(RbNode* node) {
      return twiddle::OwnerOf(node, &FreeBlock::free_tree_node_);
    }
  };

  class List : public IntrusiveLinkedList<FreeBlock, List> {
   public:
    static constexpr Node* GetNode(FreeBlock* block) {
      return &block->list_node_;
    }

    static constexpr FreeBlock* GetItem(Node* node) {
      return twiddle::OwnerOf(node, &FreeBlock::list_node_);
    }
  };

 private:
  FreeBlock(size_t size, bool prev_block_is_free);

  BlockHeader header_;
  RbNode free_tree_node_;
  List::Node list_node_;
};

}  // namespace blocks
}  // namespace jsmalloc
