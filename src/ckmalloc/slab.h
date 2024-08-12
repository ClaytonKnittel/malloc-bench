#pragma once

#include <cstdint>
#include <type_traits>

#include "src/ckmalloc/page_id.h"

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
enum class SlabType {
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

// Slab metadata class, which is stored separately from the slab it describes,
// in a metadata slab.
class Slab {
  friend class SlabManagerTest;

 public:
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
        struct {
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

class SmallSlab : public AllocatedSlab {
 public:
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;
};

class LargeSlab : public AllocatedSlab {
 public:
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;

  // Given an allocation request for `user_size` bytes, returns the number of
  // pages of the minimum-sized slab that could fit a block large enough to
  // satisfy this allocation.
  static uint32_t NPagesForBlock(size_t user_size);
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
SmallSlab* Slab::Init(PageId start_id, uint32_t n_pages);
template <>
LargeSlab* Slab::Init(PageId start_id, uint32_t n_pages);

}  // namespace ckmalloc
