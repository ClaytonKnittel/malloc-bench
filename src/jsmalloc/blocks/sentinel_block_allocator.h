#pragma once

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"

namespace jsmalloc {
namespace blocks {

class SentinelBlock {
 public:
  SentinelBlock()
      : header_(sizeof(SentinelBlock), BlockKind::kBeginOrEnd, false){};

 private:
  BlockHeader header_;
  uint8_t alignment_[12];
};

static_assert(sizeof(SentinelBlock) % 16 == 0);

/**
 * An allocator that ensures a sentinel block is always at the end of the heap.
 *
 * TODO(jtstogel): flatten all the allocators
 */
class SentinelBlockAllocator : public Allocator {
 public:
  explicit SentinelBlockAllocator(Allocator& allocator)
      : allocator_(allocator){};

  void Start() {
    void* ptr = allocator_.Allocate(sizeof(SentinelBlock));
    if (ptr == nullptr) {
      return;
    }
    new (ptr) SentinelBlock();
  }

  void* Allocate(size_t size) override {
    void* ptr = allocator_.Allocate(size);
    if (ptr == nullptr) {
      return nullptr;
    }

    auto* new_sentinel_ptr =
        twiddle::AddPtrOffset<void>(ptr, size - sizeof(SentinelBlock));
    new (new_sentinel_ptr) SentinelBlock();

    return twiddle::AddPtrOffset<void>(
        ptr, -static_cast<int32_t>(sizeof(SentinelBlock)));
  }

  void Free(void* ptr) override {}

 private:
  Allocator& allocator_;
};

}  // namespace blocks
}  // namespace jsmalloc
