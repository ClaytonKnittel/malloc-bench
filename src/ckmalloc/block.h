#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

// Blocks contain metadata regarding both free and allocated regions of memory
// from large slabs.
class Block {
  friend constexpr size_t HeaderOffset();

 public:
  uint64_t Size() const;

  void SetSize(uint64_t size);

  bool Free() const;

  // Marks this block as free, writing the footer to the end of the block and
  // setting the "prev free" bit of the next adjacent block.
  //
  // If `is_end_of_slab` is true, this block is at the end of the slab, and no
  // footer will be written.
  void MarkFree(bool is_end_of_slab = false);

  void MarkAllocated();

  class FreeBlock* ToFree();

  Block* NextAdjacentBlock();
  const Block* NextAdjacentBlock() const;

  Block* PrevAdjacentBlock();
  const Block* PrevAdjacentBlock() const;

 private:
  bool PrevFree() const;

  void SetPrevFree(bool free);

  // Returns the size of the previous block, which is stored in the footer of
  // the previous block (i.e. the 8 bytes before this block's header). This
  // method may only be called if `PrevFree()` is true.
  uint64_t PrevSize() const;

  // Writes to the footer of the previous block, which holds the size of the
  // previous block.
  void SetPrevSize(uint64_t size);

  static constexpr uint64_t kFreeBitMask = 0x1;
  static constexpr uint64_t kPrevFreeBitMask = 0x2;

  static constexpr uint64_t kSizeMask = ~(kFreeBitMask | kPrevFreeBitMask);

  // The header contains the size of the block, whether it is free, and whether
  // the previous block is free.
  uint64_t header_;
};

class FreeBlock : public Block, public LinkedListNode {};

}  // namespace ckmalloc
