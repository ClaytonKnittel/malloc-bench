#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

#include "src/jsmalloc/mallocator.h"
#include "src/jsmalloc/collections/intrusive_linked_list.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {

enum class BlockKind {
  kSmallBlock = 0,
  kLargeBlock = 1,
};

/**
 * Header common to all block kinds.
 */
struct CommonBlockHeader {
  /**
   * Size of the block in bytes.
   */
  uint32_t size;

  /**
   * The kind of this block.
   */
  BlockKind kind;

  /** Whether this block is free. */
  bool free;
};

/**
 * Header that sits just before every data region.
 */
union DataHeader {
  /** Number of bytes backward in memory to the start of the block. */
  uint32_t offset;
};

/** Returns the block containing the provided data pointer. */
CommonBlockHeader* BlockFromDataPointer(void* ptr);

class StaticAsserts;

struct SmallBlockOptions {
  size_t data_size;
  size_t bin_count;
};

/**
 * Block that holds small memory allocations.
 *
 *                SmallBlock
 * ------------------------------------------ <- 16b aligned
 * |           CommonBlockHeader            |
 * ------------------------------------------
 * |           free bins bitmap             |
 * ------------------------------------------
 * |               data size                |
 * ------------------------------------------
 * |               bin count                |
 * ------------------------------------------
 * |        IntrusiveLinkedListNode         |
 * ------------------------------------------
 * |                 bins                   |    packed array of SmallBlock::Bin
 * ------------------------------------------ <- 16b-4 aligned
 * |                unused                  |
 * ------------------------------------------ <- 16b aligned
 *
 *              SmallBlock::Bin
 * ------------------------------------------ <- 16b-4 aligned
 * |              DataHeader                |
 * ------------------------------------------ <- 16b aligned
 * |                 data                   |    16 * N - 4 bytes
 * ------------------------------------------ <- 16b-4 aligned
 */
class SmallBlock {
  friend StaticAsserts;

 public:
  /**
   * Allocates a new free block.
   */
  static SmallBlock* New(Mallocator* mallocator, SmallBlockOptions options);

  /** Frees the bin associated with the provided data pointer. */
  void Free(void* ptr);

  /** Allocates a bin and returns a pointer to the memory region. */
  void* Alloc();

  /** Whether this block is free and can be consumed. */
  bool IsFree() const;

  /** Whether this block has more memory to allocate out. */
  bool CanAlloc() const;

  /** The size of this block. */
  size_t Size() const;

  /** The size of data this block can allocate. */
  size_t DataSize() const;

  class FreeList : public IntrusiveLinkedList<SmallBlock> {
   public:
    FreeList()
        : IntrusiveLinkedList<SmallBlock>(&SmallBlock::free_list_node_) {}
  };

 private:
  explicit SmallBlock(size_t block_size, SmallBlockOptions options);

  /** Returns the minimum number of bytes needed to allocate a SmallBlock. */
  static constexpr size_t BlockSizeForBinSize(SmallBlockOptions options) {
    return math::round_16b(offsetof(SmallBlock, bins_) +
                           (offsetof(Bin, data_) + options.data_size) *
                               options.bin_count);
  }

  uint32_t InitialFreeMask() const;
  void MarkBinFree(int bin_index);
  void MarkBinUsed(int bin_index);
  void UpdateFreeBit();
  uint32_t BinSize() const;

  struct Bin {
    friend SmallBlock;
    friend StaticAsserts;

   private:
    DataHeader header_;
    uint8_t data_[];
  };

  CommonBlockHeader header_;
  uint32_t data_size_;
  uint32_t bin_count_;
  uint32_t free_bins_;
  IntrusiveLinkedList<SmallBlock>::Node free_list_node_;
  [[maybe_unused]] uint8_t unused_for_alignment_[4];
  uint8_t bins_[];
};

constexpr int kSmallBlockSizeCount = 9;
constexpr int kMaxSmallBlockDataSize = 252;

class MultiSmallBlockFreeList {
 public:
  /**
   * Finds the free list for small blocks fitting `data_size`.
   */
  SmallBlock::FreeList& Find(size_t data_size);

  /**
   * Ensure that block is in the multi free list.
   */
  void EnsureContains(SmallBlock& block);

  /**
   * Creates a new SmallBlock.
   */
  static SmallBlock* Create(Mallocator* mallocator, size_t data_size);

 private:
  SmallBlock::FreeList free_lists_[kSmallBlockSizeCount];
};

/**
 * Block for holding a large section of allocated memory.
 *
 *                LargeBlock
 * ------------------------------------------ <- 16-byte aligned
 * |           CommonBlockHeader            |
 * ------------------------------------------
 * |        IntrusiveLinkedListNode         |
 * ------------------------------------------
 * |                unused                  |    (for alignment)
 * ------------------------------------------
 * |              data header               |
 * ------------------------------------------ <- 16-byte aligned
 * |                 data                   |    16N bytes
 * ------------------------------------------
 */
class LargeBlock {
  friend StaticAsserts;

 public:
  /**
   * Allocates a new free block.
   */
  static LargeBlock* New(Mallocator* mallocator, size_t data_size);

  /** Frees the memory associated with the provided data pointer. */
  void Free(void* ptr);

  /** Allocates memory and returns a pointer to the memory region. */
  void* Alloc();

  /** Whether this block is free and can be consumed. */
  bool IsFree() const;

  /** Whether this block has more memory to allocate out. */
  bool CanAlloc() const;

  /** The size of this block. */
  size_t Size() const;

  /** The size of data this block can allocate. */
  size_t DataSize() const;

  class FreeList : public IntrusiveLinkedList<LargeBlock> {
   public:
    FreeList() : IntrusiveLinkedList(&LargeBlock::free_list_node_) {}
  };

 private:
  explicit LargeBlock(size_t block_size);

  /** The block size required to store `data_size` bytes. */
  static constexpr size_t BlockSizeForDataSize(size_t data_size) {
    return math::round_16b(offsetof(LargeBlock, data_) + data_size);
  }

  CommonBlockHeader header_;
  IntrusiveLinkedList<LargeBlock>::Node free_list_node_;
  [[maybe_unused]] uint8_t unused_for_alignment_[12];
  DataHeader data_header_;
  uint8_t data_[];
};

class StaticAsserts {
  static_assert(offsetof(LargeBlock, data_) % 16 == 0);

  static_assert(offsetof(SmallBlock, bins_) % 16 == 12);
  static_assert(offsetof(SmallBlock::Bin, data_) % 16 == 4);
};

}  // namespace jsmalloc
