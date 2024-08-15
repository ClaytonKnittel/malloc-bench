#pragma once

#include <cstdint>
#include <ostream>
#include <type_traits>

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slice.h"

namespace ckmalloc {

template <typename>
struct HasMetadataHelper : public std::false_type {};

template <>
struct HasMetadataHelper<SmallSlab> : public std::true_type {};

template <>
struct HasMetadataHelper<LargeSlab> : public std::true_type {};

template <typename S>
inline constexpr bool kHasMetadata = HasMetadataHelper<S>::value;

// The slab types are the possible variant types of slabs.
enum class SlabType : uint8_t {
  // The slab metadata is free and in the metadata freelist. It is not managing
  // any allocated slab and can be claimed for a new slab.
  kUnmapped,

  // This slab metadata is managing a free slab.
  kFree,

  // This slab metadata is managing a small block slab.
  kSmall,

  // This slab is managing a large block slab.
  kLarge,
};

std::ostream& operator<<(std::ostream& ostr, SlabType slab_type);

template <typename T>
requires std::is_integral_v<T>
class SmallSlabMetadata {
  static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>,
                "SmallSlabMetadata can only be instantiated with `uint8_t` or "
                "`uint16_t`");

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
  uint8_t FreelistNodesPerSlice(class SizeClass size_class);

  // Given a pointer to the start of the slab and a `Slice`, returns the slab ID
  // for that slice.
  SliceId<T> SliceIdForSlice(void* slab_start, FreeSlice<T>* slice);

  // Given a pointer to the start of the slab and a `SliceId`, returns a pointer
  // to the corresponding slice.
  FreeSlice<T>* SliceFromId(void* slab_start, SliceId<T> slice_id);

  // The size of allocations this slab holds.
  class SizeClass size_class_;

  // Some free slices contain up to four pointers to other free slices
  // in this slab. This number here is the count of other pointers in
  // the slice `freelist_` points to, and also the offset that the next
  // freed slice id should be placed. It ranges from 0-3.
  uint8_t freelist_node_offset_;

  // The slice id of the first slice in the freelist.
  SliceId<T> freelist_ = SliceId<T>::Nil();

  // The count of uninitialized slices. This starts off at
  // size_class_.MaxSlicesPerSlab(), and will decrease as free blocks are
  // requested and the freelist remains empty. Once this reaches zero, all
  // slices are either allocated or in the freelist, so if the freelist is empty
  // then the slab is full.
  uint16_t uninitialized_count_;

  // The count of allocated slices in this slab.
  uint16_t allocated_count_ = 0;
};

// Slab metadata class, which is stored separately from the slab it describes,
// in a metadata slab.
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
        union {
          // Metadata for 8-byte slice small slabs.
          SmallSlabMetadata<uint16_t> tiny_meta_;
          // Metadata for 16-byte+ slice small slabs.
          SmallSlabMetadata<uint8_t> small_meta_;
        } small;
        struct {
          // Tracks the total number of allocated bytes in this block.
          uint64_t allocated_bytes_;
        } large;
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
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;

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
};

class FreeSlab : public MappedSlab {
 public:
  class AllocatedSlab* ToAllocated() = delete;
  const class AllocatedSlab* ToAllocated() const = delete;
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;
};

class AllocatedSlab : public MappedSlab {
 public:
  class FreeSlab* ToFree() = delete;
  const class FreeSlab* ToFree() const = delete;
};

// Small slabs hold many duplicates of a single size of block.
class SmallSlab : public AllocatedSlab {
 public:
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;

  SizeClass SizeClass() const;

  // Tiny metadata is for 8-byte block small slabs.
  SmallSlabMetadata<uint16_t>& TinyMetadata();
  // Small metadata is for 16-byte block small slabs.
  SmallSlabMetadata<uint8_t>& SmallMetadata();
};

class LargeSlab : public AllocatedSlab {
 public:
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;

  // Given an allocation request for `user_size` bytes, returns the number of
  // pages of the minimum-sized slab that could fit a block large enough to
  // satisfy this allocation.
  static uint32_t NPagesForBlock(size_t user_size);

  // Returns the largest block size that can fit in this large slab.
  uint64_t MaxBlockSize() const;

  // Adds `n_bytes` to the total allocated byte count of the slab.
  void AddAllocation(uint64_t n_bytes);

  // Removes `n_bytes` from the total allocated byte count of the slab.
  void RemoveAllocation(uint64_t n_bytes);

  uint64_t AllocatedBytes() const;
};

// The sizes of all subtypes of slab must be equal.
static_assert(sizeof(Slab) == sizeof(UnmappedSlab),
              "Slab subtype sizes must be equal, UnmappedSlab different.");
static_assert(sizeof(Slab) == sizeof(MappedSlab),
              "Slab subtype sizes must be equal, MappedSlab different.");
static_assert(sizeof(Slab) == sizeof(FreeSlab),
              "Slab subtype sizes must be equal, FreeSlab different.");
static_assert(sizeof(Slab) == sizeof(AllocatedSlab),
              "Slab subtype sizes must be equal, AllocatedSlab different.");
static_assert(sizeof(Slab) == sizeof(SmallSlab),
              "Slab subtype sizes must be equal, SmallSlab different.");
static_assert(sizeof(Slab) == sizeof(LargeSlab),
              "Slab subtype sizes must be equal, LargeSlab different.");

template <>
UnmappedSlab* Slab::Init(UnmappedSlab* next_unmapped);
template <>
FreeSlab* Slab::Init(PageId start_id, uint32_t n_pages);
template <>
SmallSlab* Slab::Init(PageId start_id, uint32_t n_pages, SizeClass size_class);
template <>
LargeSlab* Slab::Init(PageId start_id, uint32_t n_pages);

}  // namespace ckmalloc
