#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "src/singleton_heap.h"

namespace ckmalloc {

static constexpr uint32_t kPageShift = 12;
// The size of slabs in bytes.
static constexpr size_t kPageSize = 1 << kPageShift;

constexpr uint32_t kHeapSizeShift = 29;
// NOLINTNEXTLINE(google-readability-casting)
static_assert(bench::SingletonHeap::kHeapSize == (size_t(1) << kHeapSizeShift));

// Forward declarations for concepts (to prevent circular dependencies):
enum class SlabType;

template <typename T>
concept MetadataAllocInterface =
    requires(size_t size, size_t alignment, class MappedSlab* slab) {
      { T::SlabAlloc() } -> std::convertible_to<class Slab*>;
      { T::SlabFree(slab) } -> std::same_as<void>;
      { T::Alloc(size, alignment) } -> std::convertible_to<void*>;
    };

template <typename T>
concept SlabMapInterface =
    requires(const T const_slab_map, T slab_map, class MappedSlab* slab,
             class PageId page_id) {
      {
        const_slab_map.FindSlab(page_id)
      } -> std::convertible_to<class MappedSlab*>;
      { slab_map.AllocatePath(page_id, page_id) } -> std::convertible_to<bool>;
      { slab_map.Insert(page_id, slab) } -> std::same_as<void>;
      { slab_map.InsertRange(page_id, page_id, slab) } -> std::same_as<void>;
    };

using SlabMgrAllocResult = std::pair<class PageId, class AllocatedSlab*>;

template <typename T>
concept SlabManagerInterface = requires(
    const T const_slab_mgr, T slab_mgr, class PageId page_id, const void* ptr,
    uint32_t n_pages, SlabType slab_type, class AllocatedSlab* slab) {
  { const_slab_mgr.PageStartFromId(page_id) } -> std::convertible_to<void*>;
  { const_slab_mgr.PageIdFromPtr(ptr) } -> std::convertible_to<class PageId>;
  {
    slab_mgr.Alloc(n_pages, slab_type)
  } -> std::convertible_to<std::optional<SlabMgrAllocResult>>;
  { slab_mgr.Free(slab) } -> std::same_as<void>;
};

// This is defined in `state.cc` to avoid circular dependencies.
class GlobalMetadataAlloc {
 public:
  // Allocate slab metadata and return a pointer which may be used by the
  // caller. Returns nullptr if out of memory.
  static Slab* SlabAlloc();

  // Frees slab metadata for later use.
  static void SlabFree(MappedSlab* slab);

  // Allocates raw memory from the metadata allocator which cannot be freed.
  // This is only intended for metadata allocation, never user data allocation.
  static void* Alloc(size_t size, size_t alignment);
};

}  // namespace ckmalloc
