#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace ckmalloc {

// The alignment of all allocations above the default alignment threshold.
static constexpr uint64_t kDefaultAlignment = 16;

// The alignment of small allocations (<= 8 bytes).
static constexpr uint64_t kMinAlignment = 8;

static constexpr uint32_t kPageShift = 12;
// The size of slabs in bytes.
static constexpr size_t kPageSize = 1 << kPageShift;

// The size of each mmapped heap allocaation.
static constexpr size_t kHeapSize = 512 * (1 << 20);

static constexpr uint32_t kHeapSizeShift = 29;
// NOLINTNEXTLINE(google-readability-casting)
static_assert(kHeapSize == (size_t(1) << kHeapSizeShift));

// The largest user-request size which will be allocated in small slabs. Any
// size larger will go in large blocks.
static constexpr size_t kMaxSmallSize = 128;

// If true, memory for this request will be allocated from a small slab.
inline constexpr bool IsSmallSize(size_t user_size) {
  return user_size <= kMaxSmallSize;
}

// Forward declarations for concepts (to prevent circular dependencies):
enum class SlabType : uint8_t;

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

template <typename T>
concept SlabManagerInterface =
    requires(const T const_slab_mgr, T slab_mgr, class PageId page_id,
             const void* ptr, uint32_t n_pages, uint32_t from, uint32_t to,
             class AllocatedSlab* slab, class BlockedSlab* blocked_slab) {
      { const_slab_mgr.PageStartFromId(page_id) } -> std::convertible_to<void*>;
      {
        const_slab_mgr.PageIdFromPtr(ptr)
      } -> std::convertible_to<class PageId>;
      {
        slab_mgr.template Alloc<class SmallSlab>(n_pages)
      } -> std::convertible_to<
          std::optional<std::pair<class PageId, class SmallSlab*>>>;
      {
        slab_mgr.template Alloc<class BlockedSlab>(n_pages)
      } -> std::convertible_to<
          std::optional<std::pair<class PageId, class BlockedSlab*>>>;
      {
        slab_mgr.template Alloc<class SingleAllocSlab>(n_pages)
      } -> std::convertible_to<
          std::optional<std::pair<class PageId, class SingleAllocSlab*>>>;
      {
        slab_mgr.template Carve<class BlockedSlab>(blocked_slab, from, to)
      } -> std::convertible_to<
          std::optional<std::pair<class FreeSlab*, class BlockedSlab*>>>;
      { slab_mgr.Resize(slab, n_pages) } -> std::convertible_to<bool>;
      { slab_mgr.Free(slab) } -> std::same_as<void>;
      {
        slab_mgr.FirstBlockInBlockedSlab(blocked_slab)
      } -> std::convertible_to<class Block*>;
    };

template <typename T>
concept SmallAllocatorInterface =
    requires(T small_alloc, size_t user_size, class SmallSlab* slab,
             class AllocatedSlice* slice) {
      {
        small_alloc.AllocSlice(user_size)
      } -> std::convertible_to<class AllocatedSlice*>;
      {
        small_alloc.ReallocSlice(slab, slice, user_size)
      } -> std::convertible_to<class AllocatedSlice*>;
      { small_alloc.FreeSlice(slab, slice) } -> std::same_as<void>;
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
