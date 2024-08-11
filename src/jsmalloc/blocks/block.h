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

  // A `SmallBlock` (see src/jsmalloc/blocks/small_block.h).
  kSmall = 1,

  // A `LargeBlock` (see src/jsmalloc/blocks/large_block.h).
  kLarge = 2,

  // A special block that notates the start or end of the heap.
  kBeginOrEnd = 3,
};

/**
 * Metadata common to all blocks.
 */
class BlockHeader {
 public:
  BlockHeader(uint32_t size, BlockKind kind, bool prev_block_is_free);

  /** The total size of the block. */
  uint32_t BlockSize() const;

  /** The kind of the block. */
  BlockKind Kind() const;

  /** The kind of the block. */
  bool PrevBlockIsFree() const;

  /** Whether this block has the correct magic value (debug only). */
  bool IsValid() const;

  /** Returns the block containing the provided data pointer. */
  static BlockHeader* FromDataPtr(void* ptr);

  /** Sets `PrevBlockIsFree` for the next block on the heap. */
  void SignalFreeToNextBlock(bool free);

 private:
  void SetBlockSize(uint32_t size);
  void SetKind(BlockKind kind);
  void SetPrevBlockIsFree(bool value);

  uint32_t data_;

#ifdef ENABLE_MAGIC_CHECKS
  static constexpr uint32_t kMagicValue = 0xdeadbeef;
  uint32_t magic_ = kMagicValue;
  [[maybe_unused]] uint32_t unused_for_alignment_[3];
#endif
};

}  // namespace blocks
}  // namespace jsmalloc
