#pragma once

#include <cstddef>

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/collections/intrusive_linked_list.h"

namespace jsmalloc {
namespace blocks {

class SmallBlockHelper;

class SmallBlock {
  friend SmallBlockHelper;

 public:
  /**
   * Allocates and returns a new SmallBlock that
   * can lease out data_size byte chunks.
   */
  static SmallBlock* New(FreeBlockAllocator& allocator, size_t data_size,
                         size_t bin_count);

  /** Frees the memory associated with the provided data pointer. */
  void Free(void* ptr);

  /** Allocates memory and returns a pointer to the memory region. */
  void* Alloc();

  /** Whether this block is empty and can safely be reclaimed. */
  bool IsEmpty() const;

  /** Whether this block has free bins and can support an `Alloc()`. */
  bool IsFull() const;

  /** This block's total size. */
  size_t BlockSize() const;

  /** The size of data this block can allocate. */
  size_t DataSize() const;

  /** A list of `SmallBlock` values. */
  class List : public IntrusiveLinkedList<SmallBlock> {
   public:
    List()
        : jsmalloc::IntrusiveLinkedList<SmallBlock>(&SmallBlock::list_node_) {}
  };

 private:
  SmallBlock(size_t block_size, bool prev_block_is_free, size_t data_size, size_t bin_count);
  size_t BinSize() const;
  int FreeBinIndex() const;
  void MarkBinFree(int index);
  void MarkBinUsed(int index);
  uint32_t DataOffsetForBinIndex(int index) const;
  int BinIndexForDataOffset(uint32_t offset) const;

  class Bin {
    friend SmallBlock;
    friend SmallBlockHelper;

   private:
    DataPreamble data_preamble_;
    /** Data given to the user. Size if of the form 12+16n bytes. */
    uint8_t data_[];
  };

  BlockHeader header_;
  uint16_t data_size_;
  uint16_t bin_count_;
  uint64_t free_bins_bitset_;
  IntrusiveLinkedList<SmallBlock>::Node list_node_;
  [[maybe_unused]] uint8_t alignment_[12];
  uint8_t bins_[];
};

}  // namespace blocks
}  // namespace jsmalloc
