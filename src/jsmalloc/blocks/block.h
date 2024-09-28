#pragma once

#include <cstdint>

namespace jsmalloc {
namespace blocks {

/**
 * The bytes that sit before every data region given to the user.
 */
struct DataPreamble {
  /**
   * Number of bytes backward in memory until the start of the block,
   * measured from the data pointer's location.
   */
  uint32_t offset;
};

/** Returns the DataPreamble that sits before a data pointer. */
DataPreamble* DataPreambleFromDataPtr(void* ptr);

/** The kinds of blocks that exist. */
enum class BlockKind {
  // A `FreeBlock` (see src/jsmalloc/blocks/free_block.h).
  kFree = 0,

  // An unused block free block.
  // Not really free, but not really used.
  // Blowing in the wind, undefined.
  // One day it may find itself fulfilled,
  // but until then, it will wallow, silently,
  // in the torn out pages of time.
  kLeasedFreeBlock = 4,

  // A `SmallBlock` (see src/jsmalloc/blocks/small_block.h).
  kSmall = 1,

  // A `LargeBlock` (see src/jsmalloc/blocks/large_block.h).
  kLarge = 2,

  // A special block that notates the start or end of the heap.
  kEnd = 3,
};

/**
 * Metadata common to all blocks.
 */
class BlockHeader {
 public:
  BlockHeader(uint32_t size, BlockKind kind, bool prev_block_is_free);

  /** The total size of the block. */
  uint32_t BlockSize() const {
    return block_size_ << 4;
  };

  /** The kind of the block. */
  BlockKind Kind() const;

  /** Sets the kind of the block. */
  void SetKind(BlockKind kind);

  /** Whether the block before this in memory is free. */
  bool PrevBlockIsFree() const;

  /** Whether this block has the correct magic value (debug only). */
  bool IsValid() const;

  /** Returns the block containing the provided data pointer. */
  static BlockHeader* FromDataPtr(void* ptr);

  /** Sets `PrevBlockIsFree` for the next block on the heap. */
  void SignalFreeToNextBlock(bool free);

  BlockHeader* NextBlock();

 private:
  void SetBlockSize(uint32_t size);
  void SetPrevBlockIsFree(bool value);

  BlockKind kind_          : 3;
  bool prev_block_is_free_ : 1;
  uint32_t block_size_     : 28;

#ifdef ENABLE_MAGIC_CHECKS
  static constexpr uint32_t kMagicValue = 0xdeadbeef;
  uint32_t magic_ = kMagicValue;
  [[maybe_unused]] uint32_t unused_for_alignment_[3];
#endif
};

}  // namespace blocks
}  // namespace jsmalloc
