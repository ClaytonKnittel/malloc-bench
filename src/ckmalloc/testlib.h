#pragma once

#include <cinttypes>
#include <cstddef>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class TestMetadataAllocInterface {
 public:
  virtual Slab* SlabAlloc() = 0;
  virtual void SlabFree(MappedSlab* slab) = 0;
  virtual void* Alloc(size_t size, size_t alignment) = 0;

  // Test-only function to delete memory allocted by `Alloc`.
  virtual void ClearAllAllocs() = 0;
};

class DetachedMetadataAlloc : public TestMetadataAllocInterface {
 public:
  Slab* SlabAlloc() override;
  void SlabFree(MappedSlab* slab) override;
  void* Alloc(size_t size, size_t alignment) override;

  void ClearAllAllocs() override;
};

class TestGlobalMetadataAlloc {
 public:
  static Slab* SlabAlloc();
  static void SlabFree(MappedSlab* slab);
  static void* Alloc(size_t size, size_t alignment);

  // Test-only function to delete memory allocted by `Alloc`.
  static void ClearAllAllocs();

  static void OverrideAllocator(TestMetadataAllocInterface* allocator) {
    allocator_ = allocator;
  }

 private:
  static TestMetadataAllocInterface* allocator_;
};

using TestSlabMap = SlabMapImpl<TestGlobalMetadataAlloc>;

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
  switch (slab.Type()) {
    case SlabType::kUnmapped: {
      absl::Format(&sink, "Unmapped slab metadata!");
      break;
    }
    case SlabType::kFree:
    case SlabType::kSmall: {
      const MappedSlab& mapped_slab = *slab.ToMapped();
      absl::Format(&sink, "Slab: [type=%v, pages=%" PRIu32 ", start_id=%v]",
                   mapped_slab.Type(), mapped_slab.Pages(),
                   mapped_slab.StartId());
      break;
    }
    case SlabType::kLarge: {
      const LargeSlab& large_slab = *slab.ToLarge();
      absl::Format(&sink,
                   "Slab: [type=%v, pages=%" PRIu32
                   ", start_id=%v, allocated_bytes=%" PRIu64 "]",
                   large_slab.Type(), large_slab.Pages(), large_slab.StartId(),
                   large_slab.AllocatedBytes());
      break;
    }
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

class TestHeap : private AlignedAlloc, public bench::Heap {
 public:
  explicit TestHeap(size_t n_pages)
      : AlignedAlloc(n_pages * kPageSize, kPageSize),
        bench::Heap(RegionStart(), n_pages * kPageSize) {}
};

class CkMallocTest {
 public:
  virtual ~CkMallocTest() {
    TestGlobalMetadataAlloc::ClearAllAllocs();
  }

  absl::Status FailedTest(const std::string& message) const {
    return absl::FailedPreconditionError(
        absl::StrCat(TestPrefix(), " ", message));
  }

  template <typename... Args>
  absl::Status FailedTest(const absl::FormatSpec<Args...>& fmt,
                          const Args&... args) const {
    return absl::FailedPreconditionError(
        absl::StrCat(TestPrefix(), " ", absl::StrFormat(fmt, args...)));
  }

  virtual const char* TestPrefix() const = 0;

  // Performs comprehensive validation checks on the heap. May be called
  // frequently in tests to verify the heap remains in a consistent state.
  virtual absl::Status ValidateHeap() = 0;
};

struct LargeSlabInfo {
  void* start;
  void* end;
  LargeSlab* slab;
};

absl::Status ValidateLargeSlabs(const std::vector<LargeSlabInfo>& slabs,
                                const Freelist& freelist);

}  // namespace ckmalloc
