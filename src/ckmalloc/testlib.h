#pragma once

#include <cinttypes>
#include <cstddef>

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class TestGlobalMetadataAlloc {
 public:
  static Slab* SlabAlloc();
  static void SlabFree(Slab* slab);
  static void* Alloc(size_t size, size_t alignment);

  // Test-only function to delete memory allocted by `Alloc`.
  static void ClearAllAllocs();
};

using TestSlabMap = SlabMapImpl<TestGlobalMetadataAlloc>;
// using TestSlabManager = SlabManagerImpl<TestGlobalMetadataAlloc,
// TestSlabMap>; using TestMetadataManager = MetadataManagerImpl<TestSlabMap,
// TestSlabManager>;

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
    case SlabType::kMetadata: {
      sink.Append("kMetadata");
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
  absl::Format(&sink, "Slab: [type=%v, pages=%" PRIu32 ", start_id=%v]",
               slab.Type(), slab.Pages(), slab.StartId());
}

template <typename Sink>
void AbslStringify(Sink& sink, const Slab* slab) {
  if (slab == nullptr) {
    sink.Append("[nullptr]");
  } else {
    absl::Format(&sink, "%v", *slab);
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

class CkMallocTest : public ::testing::Test {
 public:
  ~CkMallocTest() override {
    TestGlobalMetadataAlloc::ClearAllAllocs();
  }

  // Performs comprehensive validation checks on the heap. May be called
  // frequently in tests to verify the heap remains in a consistent state.
  virtual absl::Status ValidateHeap() = 0;
};

}  // namespace ckmalloc
