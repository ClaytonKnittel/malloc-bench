#pragma once

#include <cinttypes>
#include <cstddef>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/heap_iterator.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/util.h"
#include "src/heap_factory.h"
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

  static uint64_t TotalAllocs();

  // Test-only function to delete memory allocted by `Alloc`.
  static void ClearAllAllocs();

  static void OverrideAllocator(TestMetadataAllocInterface* allocator);
  static void ClearAllocatorOverride();

 private:
  static TestMetadataAllocInterface* allocator_;
  static uint64_t n_allocs_;
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
    case SlabType::kBlocked: {
      sink.Append("kBlocked");
      break;
    }
    case SlabType::kSingleAlloc: {
      sink.Append("kSingleAlloc");
      break;
    }
    case SlabType::kMmap: {
      sink.Append("kMmap");
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
    case SlabType::kSmall:
    case SlabType::kSingleAlloc:
    case SlabType::kMmap: {
      const MappedSlab& mapped_slab = *slab.ToMapped();
      absl::Format(&sink, "Slab: [type=%v, pages=%" PRIu32 ", start_id=%v]",
                   mapped_slab.Type(), mapped_slab.Pages(),
                   mapped_slab.StartId());
      break;
    }
    case SlabType::kBlocked: {
      const BlockedSlab& blocked_slab = *slab.ToBlocked();
      absl::Format(&sink,
                   "Slab: [type=%v, pages=%" PRIu32
                   ", start_id=%v, allocated_bytes=%" PRIu64 "]",
                   blocked_slab.Type(), blocked_slab.Pages(),
                   blocked_slab.StartId(), blocked_slab.AllocatedBytes());
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
    } else if (block.IsExactSize()) {
      absl::Format(&sink,
                   "Block %p: [free, size=%" PRIu64 ", prev=%p, next=%p]",
                   &block, block.Size(), block.ToExactSize()->Prev(),
                   block.ToExactSize()->Next());
    } else {
      absl::Format(&sink,
                   "Block %p: [free, size=%" PRIu64
                   ", left=%p, right=%p, parent=%p]",
                   &block, block.Size(), block.ToTree()->Left(),
                   block.ToTree()->Right(), block.ToTree()->Parent());
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

class TestHeapFactory : public bench::HeapFactory {
 public:
  TestHeapFactory() = default;

  explicit TestHeapFactory(size_t initial_size) {
    auto result = NewInstance(initial_size);
    CK_ASSERT_TRUE(result.ok());
  }
  TestHeapFactory(size_t initial_size1, size_t initial_size2) {
    auto result1 = NewInstance(initial_size1);
    CK_ASSERT_TRUE(result1.ok());
    auto result2 = NewInstance(initial_size2);
    CK_ASSERT_TRUE(result2.ok());
  }

  ~TestHeapFactory() override = default;

 protected:
  absl::StatusOr<std::unique_ptr<bench::Heap>> MakeHeap(size_t size) override;
};

using TestHeapIterator = HeapIteratorImpl<TestSlabMap>;

class TestSysAlloc : public SysAlloc {
 public:
  explicit TestSysAlloc(bench::HeapFactory* heap_factory);

  static TestSysAlloc* NewInstance(bench::HeapFactory* heap_factory);

  static void Reset();

  void* Mmap(void* start_hint, size_t size) override;

  void Munmap(void* ptr, size_t size) override;

  void Sbrk(void* heap_start, size_t increment, void* current_end) override;

  bench::Heap* HeapFromStart(void* heap_start);

 private:
  bench::HeapFactory* heap_factory_;

  absl::flat_hash_map<void*, bench::Heap*> heap_map_;
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

}  // namespace ckmalloc
