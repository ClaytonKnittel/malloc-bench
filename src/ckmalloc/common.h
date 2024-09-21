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
static constexpr size_t kMaxSmallSize = 512;

// If true, memory for this request will be allocated from a small slab.
inline constexpr bool IsSmallSize(size_t user_size) {
  return user_size <= kMaxSmallSize;
}

// Forward declarations for concepts (to prevent circular dependencies):
enum class SlabType : uint8_t;

// Strongly-typed void, to avoid accidental auto-conversion from pointer-to-T to
// void.
struct Void {};

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
             class PageId page_id, class SizeClass size_class) {
      {
        const_slab_map.FindSizeClass(page_id)
      } -> std::convertible_to<class SizeClass>;
      {
        const_slab_map.FindSlab(page_id)
      } -> std::convertible_to<class MappedSlab*>;
      { slab_map.AllocatePath(page_id, page_id) } -> std::convertible_to<bool>;
      { slab_map.Insert(page_id, slab) } -> std::same_as<void>;
      { slab_map.Insert(page_id, slab, size_class) } -> std::same_as<void>;
      { slab_map.InsertRange(page_id, page_id, slab) } -> std::same_as<void>;
      {
        slab_map.InsertRange(page_id, page_id, slab, size_class)
      } -> std::same_as<void>;
    };

template <typename T>
concept SlabManagerInterface =
    requires(const T const_slab_mgr, T slab_mgr, class PageId page_id,
             const void* ptr, uint32_t n_pages, class AllocatedSlab* slab,
             class BlockedSlab* blocked_slab) {
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
      { slab_mgr.Resize(slab, n_pages) } -> std::convertible_to<bool>;
      { slab_mgr.Free(slab) } -> std::same_as<void>;
      {
        slab_mgr.FirstBlockInBlockedSlab(blocked_slab)
      } -> std::convertible_to<class Block*>;
    };

template <typename T>
concept MetadataManagerInterface =
    requires(T meta_mgr, size_t size, class MappedSlab* slab) {
      { meta_mgr.Alloc(size, size) } -> std::convertible_to<void*>;
      { meta_mgr.NewSlabMeta() } -> std::convertible_to<class Slab*>;
      { meta_mgr.FreeSlabMeta(slab) } -> std::same_as<void>;
    };

template <typename T>
concept SmallAllocatorInterface = requires(T small_alloc, size_t user_size,
                                           class SmallSlab* slab, Void* ptr) {
  { small_alloc.AllocSmall(user_size) } -> std::convertible_to<Void*>;
  {
    small_alloc.ReallocSmall(slab, ptr, user_size)
  } -> std::convertible_to<Void*>;
  { small_alloc.FreeSmall(slab, ptr) } -> std::same_as<void>;
};

template <typename T>
concept LargeAllocatorInterface = requires(T large_alloc, size_t user_size,
                                           class LargeSlab* slab, Void* ptr) {
  { large_alloc.AllocLarge(user_size) } -> std::convertible_to<Void*>;
  {
    large_alloc.ReallocLarge(slab, ptr, user_size)
  } -> std::convertible_to<Void*>;
  { large_alloc.FreeLarge(slab, ptr) } -> std::same_as<void>;
};

template <typename T>
concept MainAllocatorInterface =
    requires(T main_alloc, size_t user_size, Void* ptr) {
      { main_alloc.Alloc(user_size) } -> std::convertible_to<Void*>;
      { main_alloc.Realloc(ptr, user_size) } -> std::convertible_to<Void*>;
      { main_alloc.Free(ptr) } -> std::same_as<void>;
      { main_alloc.AllocSize(ptr) } -> std::convertible_to<size_t>;
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
