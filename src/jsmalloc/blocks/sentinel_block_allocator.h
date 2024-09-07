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
  [[maybe_unused]] uint8_t alignment_[12];
};

static_assert(sizeof(SentinelBlock) % 16 == 0);

/**
 * A heap maintaining sentinel blocks at both ends.
 */
class SentinelBlockHeap {
 public:
  explicit SentinelBlockHeap(MemRegion& mem_region) : mem_region_(mem_region){};

  void Init() {
    void* ptr = mem_region_.Extend(sizeof(SentinelBlock));
    if (ptr == nullptr) {
      return;
    }
    new (ptr) SentinelBlock();
  }

  void* sbrk(intptr_t increment) {
    void* ptr = mem_region_.Extend(increment);
    if (ptr == nullptr) {
      return nullptr;
    }

    auto* new_sentinel_ptr =
        twiddle::AddPtrOffset<void>(ptr, increment - sizeof(SentinelBlock));
    new (new_sentinel_ptr) SentinelBlock();

    return twiddle::AddPtrOffset<void>(
        ptr, -static_cast<int32_t>(sizeof(SentinelBlock)));
  }

 private:
  MemRegion& mem_region_;
};

}  // namespace blocks
}  // namespace jsmalloc
