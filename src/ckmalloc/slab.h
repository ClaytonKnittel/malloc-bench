#pragma once

#include <cstdint>
#include <ostream>
#include <type_traits>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slice.h"
#include "src/ckmalloc/slice_id.h"

namespace ckmalloc {

template <typename>
struct IsManagedSlabHelper : public std::false_type {};

template <>
struct IsManagedSlabHelper<class SmallSlab> : public std::true_type {};
template <>
struct IsManagedSlabHelper<class BlockedSlab> : public std::true_type {};
template <>
struct IsManagedSlabHelper<class SingleAllocSlab> : public std::true_type {};

template <typename S>
inline constexpr bool kIsManagedSlab = IsManagedSlabHelper<S>::value;

template <typename>
struct HasOneAllocationHelper : public std::false_type {};

template <>
struct HasOneAllocationHelper<class FreeSlab> : public std::true_type {};
template <>
struct HasOneAllocationHelper<class SingleAllocSlab> : public std::true_type {};
template <>
struct HasOneAllocationHelper<class MmapSlab> : public std::true_type {};

template <typename S>
inline constexpr bool kHasOneAllocation = HasOneAllocationHelper<S>::value;

inline constexpr bool HasOneAllocation(SlabType type);

template <typename S>
concept HasSizeClassT = requires(S s) {
  { s.SizeClass() } -> std::convertible_to<SizeClass>;
};

// The slab types are the possible variant types of slabs.
enum class SlabType : uint8_t {
  // The slab metadata is free and in the metadata freelist. It is not managing
  // any allocated slab and can be claimed for a new slab.
  kUnmapped,

  // This slab metadata is managing a free slab.
  kFree,

  // This slab metadata is managing a small block slab.
  kSmall,

  // This slab is managing a blocked slab.
  kBlocked,

  // This slab is managing a single allocation of a page-size multiple (or
  // nearly page-size multiple) block.
  kSingleAlloc,

  // This slab is managing an entire mmapped region which contains a single
  // allocated block.
  kMmap,
};  // namespace ckmalloc

std::ostream& operator<<(std::ostream& ostr, SlabType slab_type);

inline constexpr bool HasOneAllocation(SlabType type) {
  CK_ASSERT_NE(type, SlabType::kUnmapped);

  switch (type) {
    case SlabType::kSmall:
    case SlabType::kBlocked: {
      return false;
    }
    case SlabType::kFree:
    case SlabType::kSingleAlloc:
    case SlabType::kMmap: {
      return true;
    }
    case SlabType::kUnmapped: {
      CK_UNREACHABLE();
    }
  }
}

template <typename T>
requires std::is_integral_v<T>
class SmallSlabMetadata {
  static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>,
                "SmallSlabMetadata can only be instantiated with `uint8_t` or "
                "`uint16_t`");

  friend class HeapPrinter;

  friend constexpr size_t TinySizeClassOffset();
  friend constexpr size_t SmallSizeClassOffset();

 public:
  explicit SmallSlabMetadata(SizeClass size_class);

  SizeClass SizeClass() const {
    return size_class_;
  }

  // If true, all slices are free in this slab.
  bool Empty() const;

  // If true, all slices are allocated in this slab.
  bool Full() const;

  // Returns the count of allocated slices in this slab.
  uint32_t AllocatedSlices() const;

  // Iterates over the free slices in this slab, calling `fn(slice_id)` for each
  // free slice.
  template <typename Fn>
  void IterateSlices(void* slab_start, Fn&& fn) const;

  // Given a pointer to the start of this slab, pops the next slice off the
  // freelist and updates the freelist accordingly, returning the newly
  // allocated slice.
  AllocatedSlice* PopSlice(void* slab_start);

  // Given a pointer to the start of this slab, pushes the given `FreeSlice`
  // onto the stack of free slices, so it may be allocated in the future.
  void PushSlice(void* slab_start, FreeSlice<T>* slice);

 private:
  // Returns the number of freelist items a single slice in the freelist can
  // hold.
  uint8_t FreelistNodesPerSlice(class SizeClass size_class) const;

  // Given a pointer to the start of the slab and a `Slice`, returns the slab ID
  // for that slice.
  SliceId<T> SliceIdForSlice(void* slab_start, FreeSlice<T>* slice);

  // Given a pointer to the start of the slab and a `SliceId`, returns a pointer
  // to the corresponding slice.
  FreeSlice<T>* SliceFromId(void* slab_start, SliceId<T> slice_id) const;

  // The size of allocations this slab holds.
  class SizeClass size_class_;

  // Some free slices contain up to four pointers to other free slices
  // in this slab. This number here is the count of other pointers in
  // the slice `freelist_` points to, and also the offset that the next
  // freed slice id should be placed. It ranges from 0-3.
  uint8_t freelist_node_offset_;

  // The count of uninitialized slices. This starts off at
  // size_class_.MaxSlicesPerSlab(), and will decrease as free blocks are
  // requested and the freelist remains empty. Once this reaches zero, all
  // slices are either allocated or in the freelist, so if the freelist is empty
  // then the slab is full.
  uint16_t uninitialized_count_;

  // The count of allocated slices in this slab.
  uint16_t allocated_count_ = 0;

  // The slice id of the first slice in the freelist.
  SliceId<T> freelist_ = SliceId<T>::Nil();
};

// Slab metadata class, which is stored separately from the slab it describes,
// in a metadata slab.
// TODO: Maybe make multiple of cache line size?
class Slab {
  friend class SlabManagerTest;
  friend constexpr size_t TinySizeClassOffset();
  friend constexpr size_t SmallSizeClassOffset();

 public:
  // For small slabs with slice sizes at or below this value, 16-byte slice IDs
  // are used.
  static constexpr uint64_t kMaxUse16ByteSliceId = 16;

  // Initializes this slab to the given slab sub-type, returning a pointer to
  // `this` down-cast to the specialized type.
  template <typename S, typename... Args>
  S* Init(Args...) = delete;

  SlabType Type() const {
    return type_;
  }

  class UnmappedSlab* ToUnmapped();
  const class UnmappedSlab* ToUnmapped() const;

  class MappedSlab* ToMapped();
  const class MappedSlab* ToMapped() const;

  class FreeSlab* ToFree();
  const class FreeSlab* ToFree() const;

  class AllocatedSlab* ToAllocated();
  const class AllocatedSlab* ToAllocated() const;

  class SmallSlab* ToSmall();
  const class SmallSlab* ToSmall() const;

  class LargeSlab* ToLarge();
  const class LargeSlab* ToLarge() const;

  class BlockedSlab* ToBlocked();
  const class BlockedSlab* ToBlocked() const;

  class SingleAllocSlab* ToSingleAlloc();
  const class SingleAllocSlab* ToSingleAlloc() const;

  class MmapSlab* ToMmap();
  const class MmapSlab* ToMmap() const;

 protected:
  Slab() {}

  SlabType type_;

  union {
    struct {
      // Unmapped slabs are held in a singly-linked freelist managed by the
      // metadata manager.
      class UnmappedSlab* next_;
    } unmapped;

    struct {
      PageId id_;
      uint32_t n_pages_;

      union {
        struct {
        } free;
        struct {
          union {
            // Metadata for <= 16-byte slice small slabs.
            SmallSlabMetadata<uint16_t> tiny_meta_;
            // Metadata for > 16-byte slice small slabs.
            SmallSlabMetadata<uint8_t> small_meta_;
          };

          // A doubly-linked list of small slabs of equal size with free space.
          PageId next_free_;
          PageId prev_free_;
        } small;
        struct {
          // Tracks the total number of allocated bytes in this block.
          uint64_t allocated_bytes_;
        } large;
        struct {
        } page_multiple;
        struct {
        } mmap;
      };
    } mapped;
  };
};

class UnmappedSlab : public Slab {
 public:
  class MappedSlab* ToMapped() = delete;
  const class MappedSlab* ToMapped() const = delete;
  class FreeSlab* ToFree() = delete;
  const class FreeSlab* ToFree() const = delete;
  class AllocatedSlab* ToAllocated() = delete;
  const class AllocatedSlab* ToAllocated() const = delete;
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;
  class BlockedSlab* ToBlocked() = delete;
  const class BlockedSlab* ToBlocked() const = delete;
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;
  class SingleAllocSlab* ToSingleAlloc() = delete;
  const class SingleAllocSlab* ToSingleAlloc() const = delete;
  class MmapSlab* ToMmap() = delete;
  const class MmapSlab* ToMmap() const = delete;

  // Returns the next unmapped slab in the freelist.
  UnmappedSlab* NextUnmappedSlab();

  // Returns the next unmapped slab in the freelist.
  const UnmappedSlab* NextUnmappedSlab() const;

  void SetNextUnmappedSlab(UnmappedSlab* next);
};

class MappedSlab : public Slab {
 public:
  class UnmappedSlab* ToUnmapped() = delete;
  const class UnmappedSlab* ToUnmapped() const = delete;

  // Returns the `PageId` of the first page in this slab.
  PageId StartId() const;

  // Returns the `PageId` of the last page in this slab.
  PageId EndId() const;

  // Returns the number of pages that this slab manages. This slab must not be a
  // freed slab metadata.
  uint32_t Pages() const;

  // Changes the size of the slab to `n_pages`. This should only be called by
  // the slab manager.
  void SetSize(uint32_t n_pages);

  // If true, this slab type has a size class, i.e. all contained blocks are the
  // same size.
  bool HasSizeClass() const;
};

class FreeSlab : public MappedSlab {
 public:
  class AllocatedSlab* ToAllocated() = delete;
  const class AllocatedSlab* ToAllocated() const = delete;
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;
  class BlockedSlab* ToBlocked() = delete;
  const class BlockedSlab* ToBlocked() const = delete;
  class SingleAllocSlab* ToSingleAlloc() = delete;
  const class SingleAllocSlab* ToSingleAlloc() const = delete;
  class MmapSlab* ToMmap() = delete;
  const class MmapSlab* ToMmap() const = delete;
};

class AllocatedSlab : public MappedSlab {
 public:
  class FreeSlab* ToFree() = delete;
  const class FreeSlab* ToFree() const = delete;
};

// Small slabs hold many duplicates of a single size of block.
class SmallSlab : public AllocatedSlab {
  friend class HeapPrinter;

 public:
  class BlockedSlab* ToBlocked() = delete;
  const class BlockedSlab* ToBlocked() const = delete;
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;
  class SingleAllocSlab* ToSingleAlloc() = delete;
  const class SingleAllocSlab* ToSingleAlloc() const = delete;
  class MmapSlab* ToMmap() = delete;
  const class MmapSlab* ToMmap() const = delete;

  SizeClass SizeClass() const;

  // If true, all slices are free in this slab.
  bool Empty() const;

  // If true, all slices are allocated in this slab.
  bool Full() const;

  // Returns the count of allocated slices in this slab.
  uint32_t AllocatedSlices() const;

  template <typename Fn>
  void IterateSlices(void* slab_start, Fn&& fn) const;

  // Given a pointer to the start of this slab, pops the next slice off the
  // freelist and updates the freelist accordingly, returning the newly
  // allocated slice.
  AllocatedSlice* PopSlice(void* slab_start);

  // Given a pointer to the start of this slab, pushes the given `FreeSlice`
  // onto the stack of free slices, so it may be allocated in the future.
  void PushSlice(void* slab_start, AllocatedSlice* slice);

  PageId NextFree() const;
  void SetNextFree(PageId next);

  PageId PrevFree() const;
  void SetPrevFree(PageId prev);

 private:
  // If true, this slab manages slices <= 16 bytes, which are classified as tiny
  // slices.
  bool IsTiny() const;
};

class LargeSlab : public AllocatedSlab {
 public:
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;
  class MmapSlab* ToMmap() = delete;
  const class MmapSlab* ToMmap() const = delete;
};

class BlockedSlab : public LargeSlab {
 public:
  class SingleAllocSlab* ToSingleAlloc() = delete;
  const class SingleAllocSlab* ToSingleAlloc() const = delete;

  // Given a block size, returns the number of pages of the minimum-sized slab
  // that could fit a block of this size.
  static uint32_t NPagesForBlock(uint64_t block_size);

  // Given a pointer to the start of a blocked slab, returns a pointer to the
  // first block.
  static Block* FirstBlock(void* slab_start);

  // Returns the largest block size that can fit in this large slab.
  uint64_t MaxBlockSize() const;

  // Adds `n_bytes` to the total allocated byte count of the slab.
  void AddAllocation(uint64_t n_bytes);

  // Removes `n_bytes` from the total allocated byte count of the slab.
  void RemoveAllocation(uint64_t n_bytes);

  uint64_t AllocatedBytes() const;
};

class SingleAllocSlab : public LargeSlab {
 public:
  BlockedSlab* ToBlocked() = delete;
  const BlockedSlab* ToBlocked() const = delete;

  // Returns the number of pages needed to hold an allocation of the given size
  // in a single-alloc slab.
  static uint32_t NPagesForAlloc(size_t user_size);

  // Returns true if this size is suitable to be allocated within a
  // page-multiple slab.
  static bool SizeSuitableForSingleAlloc(size_t user_size);
};

// Mmap slabs are slabs holding a single allocation in its own mmapped region of
// memory.
class MmapSlab : public AllocatedSlab {
 public:
  BlockedSlab* ToBlocked() = delete;
  const BlockedSlab* ToBlocked() const = delete;
  SingleAllocSlab* ToSingleAlloc() = delete;
  const SingleAllocSlab* ToSingleAlloc() const = delete;
};

// The sizes of all subtypes of slab must be equal.
static_assert(sizeof(Slab) == sizeof(UnmappedSlab),
              "Slab subtype sizes must be equal, UnmappedSlab different.");
static_assert(sizeof(Slab) == sizeof(FreeSlab),
              "Slab subtype sizes must be equal, FreeSlab different.");
static_assert(sizeof(Slab) == sizeof(SmallSlab),
              "Slab subtype sizes must be equal, SmallSlab different.");
static_assert(sizeof(Slab) == sizeof(BlockedSlab),
              "Slab subtype sizes must be equal, BlockedSlab different.");
static_assert(sizeof(Slab) == sizeof(SingleAllocSlab),
              "Slab subtype sizes must be equal, SingleAllocSlab different.");

template <>
UnmappedSlab* Slab::Init(UnmappedSlab* next_unmapped);
template <>
FreeSlab* Slab::Init(PageId start_id, uint32_t n_pages);
template <>
SmallSlab* Slab::Init(PageId start_id, uint32_t n_pages, SizeClass size_class);
template <>
BlockedSlab* Slab::Init(PageId start_id, uint32_t n_pages);
template <>
SingleAllocSlab* Slab::Init(PageId start_id, uint32_t n_pages);
template <>
MmapSlab* Slab::Init(PageId start_id, uint32_t n_pages);

template <typename T>
requires std::is_integral_v<T>
template <typename Fn>
void SmallSlabMetadata<T>::IterateSlices(void* slab_start, Fn&& fn) const {
  // Iterate over slices in the freelist.
  SliceId<T> node = freelist_;
  uint32_t offset = freelist_node_offset_;
  while (node != SliceId<T>::Nil()) {
    FreeSlice<T>* slice = SliceFromId(slab_start, node);
    if (offset == 0) {
      fn(node.Id());
      node = slice->IdAt(0);
      offset = FreelistNodesPerSlice(size_class_) - 1;
    } else {
      fn(slice->IdAt(offset).Id());
      offset--;
    }
  }

  // Iterate over all uninitialized slices.
  for (uint32_t i = size_class_.MaxSlicesPerSlab() - uninitialized_count_;
       i < size_class_.MaxSlicesPerSlab(); i++) {
    fn(SliceId<T>::FromIdx(i).Id());
  }
}

template <typename Fn>
void SmallSlab::IterateSlices(void* slab_start, Fn&& fn) const {
  CK_ASSERT_EQ(type_, SlabType::kSmall);
  if (IsTiny()) {
    mapped.small.tiny_meta_.IterateSlices(slab_start, fn);
  } else {
    mapped.small.small_meta_.IterateSlices(slab_start, fn);
  }
}

}  // namespace ckmalloc
