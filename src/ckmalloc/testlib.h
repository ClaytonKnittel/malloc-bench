#pragma once

#include <cinttypes>
#include <cstddef>

#include "absl/strings/str_format.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class TestGlobalMetadataAlloc {
 public:
  static Slab* SlabAlloc();
  static void SlabFree(MappedSlab* slab);
  static void* Alloc(size_t size, size_t alignment);

  // Test-only function to delete memory allocted by `Alloc`.
  static void ClearAllAllocs();
};

using TestSlabMap = SlabMapImpl<TestGlobalMetadataAlloc>;

template <typename Sink>
void AbslStringify(Sink& sink, const PageId& page_id) {
  absl::Format(&sink, "%" PRIu32, page_id.Idx());
}

template <typename Sink>
void AbslStringify(Sink& sink, SlabType slab_type) {
  switch (slab_type) {
    case SlabType::kUnmapped: {
      sink.Append("kUnmapped");
      break;
    }
    case SlabType::kFree: {
      sink.Append("kFree");
      break;
    }
    case SlabType::kSmall: {
      sink.Append("kSmall");
      break;
    }
    case SlabType::kLarge: {
      sink.Append("kLarge");
      break;
    }
  }
}

template <typename Sink>
void AbslStringify(Sink& sink, const Slab& slab) {
  if (slab.Type() == SlabType::kUnmapped) {
    absl::Format(&sink, "Unmapped slab metadata!");
  } else {
    const MappedSlab& mapped_slab = *slab.ToMapped();
    absl::Format(&sink, "Slab: [type=%v, pages=%" PRIu32 ", start_id=%v]",
                 mapped_slab.Type(), mapped_slab.Pages(),
                 mapped_slab.StartId());
  }
}

template <typename Sink>
void AbslStringify(Sink& sink, const Slab* slab) {
  if (slab == nullptr) {
    sink.Append("[nullptr]");
  } else {
    absl::Format(&sink, "%v", *slab);
  }
}

template <typename Sink>
void AbslStringify(Sink& sink, const Block& block) {
  if (block.Free()) {
    if (block.IsUntracked()) {
      absl::Format(&sink, "Block %p: [untracked, size=%" PRIu64 "]", &block,
                   block.Size());
    } else {
      absl::Format(
          &sink, "Block %p: [free, size=%" PRIu64 ", prev=%p, next=%p]", &block,
          block.Size(), block.ToTracked()->Prev(), block.ToTracked()->Next());
    }
  } else {
    absl::Format(&sink, "Block %p: [allocated, size=%" PRIu64 ", prev_free=%s]",
                 &block, block.Size(), (block.PrevFree() ? "true" : "false"));
  }
}

class TestHeap : public bench::Heap {
 public:
  static constexpr size_t kMaxNumPages = 64;
  static constexpr size_t kMaxHeapSize = kMaxNumPages * kPageSize;

  explicit TestHeap(size_t n_pages)
      : bench::Heap(&memory_region_,
                    std::min(n_pages * kPageSize, kMaxHeapSize)) {
    assert(n_pages <= kMaxNumPages);
  }

 private:
  // TODO: dynamically allocate
  uint8_t memory_region_[kMaxHeapSize];
};

class CkMallocTest {
 public:
  virtual ~CkMallocTest() {
    TestGlobalMetadataAlloc::ClearAllAllocs();
  }

  // Performs comprehensive validation checks on the heap. May be called
  // frequently in tests to verify the heap remains in a consistent state.
  virtual absl::Status ValidateHeap() = 0;
};

}  // namespace ckmalloc
