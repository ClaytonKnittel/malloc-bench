#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "src/singleton_heap.h"

namespace ckmalloc {

static constexpr uint32_t kPageShift = 12;
// The size of slabs in bytes.
static constexpr size_t kPageSize = 1 << kPageShift;

constexpr uint32_t kHeapSizeShift = 29;
// NOLINTNEXTLINE(google-readability-casting)
static_assert(bench::SingletonHeap::kHeapSize == (size_t(1) << kHeapSizeShift));

template <typename T>
concept MetadataAllocInterface =
    requires(size_t size, size_t alignment, class Slab* slab) {
      { T::SlabAlloc() } -> std::convertible_to<class Slab*>;
      { T::SlabFree(slab) } -> std::same_as<void>;
      { T::Alloc(size, alignment) } -> std::convertible_to<void*>;
    };

// This is defined in `state.cc` to avoid circular dependencies.
class GlobalMetadataAlloc {
 public:
  // Allocate slab metadata and return a pointer which may be used by the
  // caller. Returns nullptr if out of memory.
  static Slab* SlabAlloc();

  // Frees slab metadata for later use.
  static void SlabFree(Slab* slab);

  // Allocates raw memory from the metadata allocator which cannot be freed.
  // This is only intended for metadata allocation, never user data allocation.
  static void* Alloc(size_t size, size_t alignment);
};

}  // namespace ckmalloc
