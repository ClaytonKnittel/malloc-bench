#pragma once

#include <cstddef>
#include <cstdint>

#include "src/jsmalloc/collections/intrusive_linked_list.h"
#include "src/jsmalloc/util/math.h"

namespace jsmalloc {

class Block {
 public:
  explicit Block(size_t size) : size_(size) {}

  /**
   * Returns the amount of space needed for a Block holding data_size bytes.
   *
   * Block sizes will always be a multiple of 16 bytes.
   */
  static size_t SizeForUserData(size_t data_size) {
    return math::round_16b(sizeof(Block) + data_size);
  }

  static Block* FromDataPtr(void* data) {
    return reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(data) -
                                    Block::DataOffset());
  }

  void* Data() {
    return data_;
  }

  /** Returns the offset of data within a Block. */
  static size_t DataOffset() {
    return offsetof(Block, data_);
  }

  size_t DataSize() const {
    return size_ - DataOffset();
  }

  size_t Size() const {
    return size_;
  }

  class FreeList : public IntrusiveLinkedList<Block> {
   public:
    FreeList() : IntrusiveLinkedList(&Block::free_list_node_) {}
  };

 private:
  uint32_t size_;
  IntrusiveLinkedList<Block>::Node free_list_node_;
  uint8_t data_[];
};

}  // namespace jsmalloc
