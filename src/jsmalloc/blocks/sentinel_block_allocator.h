#pragma once

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {

class SentinelBlock {
 public:
  SentinelBlock() : header_(sizeof(SentinelBlock), BlockKind::kEnd, false){};

  BlockHeader* Header() {
    return &header_;
  }

 private:
  BlockHeader header_;
  [[maybe_unused]] uint8_t alignment_[12];
};

static_assert(sizeof(SentinelBlock) % 16 == 0);

/**
 * A heap maintaining a sentinel block at its end.
 */
class SentinelBlockHeap {
 public:
  explicit SentinelBlockHeap(MemRegion* mem_region,
                             MemRegionAllocator* allocator)
      : mem_region_(mem_region), allocator_(allocator){};

  void Init() {
    void* ptr = allocator_->Extend(mem_region_, sizeof(SentinelBlock));
    if (ptr == nullptr) {
      return;
    }
    new (ptr) SentinelBlock();
  }

  SentinelBlock* sbrk(intptr_t increment) {
    void* ptr = allocator_->Extend(mem_region_, increment);
    if (ptr == nullptr) {
      return nullptr;
    }

    auto* new_sentinel_ptr =
        twiddle::AddPtrOffset<void>(ptr, increment - sizeof(SentinelBlock));
    new (new_sentinel_ptr) SentinelBlock();

    return twiddle::AddPtrOffset<SentinelBlock>(
        ptr, -static_cast<int32_t>(sizeof(SentinelBlock)));
  }

  void* Start() {
    return mem_region_->Start();
  }

  void* End() {
    return twiddle::AddPtrOffset<void>(
        mem_region_->End(), -static_cast<int32_t>(sizeof(SentinelBlock)));
  }

 private:
  MemRegion* mem_region_;
  MemRegionAllocator* allocator_;
};

}  // namespace blocks
}  // namespace jsmalloc
