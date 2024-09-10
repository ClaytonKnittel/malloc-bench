#pragma once

#include <cstddef>
#include <cstdint>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/collections/intrusive_linked_list.h"
#include "src/jsmalloc/util/bitset.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace blocks {

class SmallBlockHelper;

class SmallBlock {
 public:
  /**
   * Returns a new SmallBlock that can lease out data_size byte chunks.
   */
  static SmallBlock* Init(FreeBlock* block, size_t bin_size, size_t bin_count);

  /**
   * The block size required for the provided configuration.
   */
  static constexpr size_t RequiredSize(size_t bin_size, size_t bin_count) {
    // data_ holds both a bitset and the actual data bins alloc'd to the user,
    // so add them up, ensuring that data is 16-byte aligned.
    return math::round_16b(offsetof(SmallBlock, data_) +
                           BitSet::RequiredSize(bin_count)) +
           bin_size * bin_count;
  }

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
  SmallBlock(size_t block_size, bool prev_block_is_free, size_t bin_size,
             size_t bin_count);

  using BitSet = BitSet1024;

  size_t BinSize() const;
  int FreeBinIndex() const;

  void MarkBinFree(int index);
  void MarkBinUsed(int index);

  void* DataPtrForBinIndex(int index);
  int BinIndexForDataPtr(void* ptr) const;

  uint8_t* MutableDataRegion();
  const uint8_t* DataRegion() const;

  BitSet* MutableUsedBinBitSet();
  const BitSet* UsedBinBitSet() const;
  size_t UsedBinBitSetSize() const;

  BlockHeader header_;
  uint16_t bin_size_;
  uint16_t bin_count_;
  IntrusiveLinkedList<SmallBlock>::Node list_node_;
  uint16_t used_bin_count_ = 0;
  alignas(16) uint8_t data_[];
};

}  // namespace blocks
}  // namespace jsmalloc
