#pragma once

#include <cstdint>

#include "src/ckmalloc/page_id.h"

namespace ckmalloc {

// The slab types are the possible variant types of slabs.
enum class SlabType {
  // The slab metadata is free and in the metadata freelist. It is not managing
  // any allocated slab and can be claimed for a new slab.
  kUnmapped,

  // This slab metadata is managing a free slab.
  kFree,

  // This slab metadata is managing a metadata slab.
  kMetadata,

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
  // Returns a pointer to `this` which has been down-cast to `UnmappedSlab`.
  class UnmappedSlab* InitUnmappedSlab(
      class UnmappedSlab* next_unmapped = nullptr);

  // Returns a pointer to `this` which has been down-cast to `FreeSlab`.
  class FreeSlab* InitFreeSlab(PageId start_id, uint32_t n_pages);

  // Returns a pointer to `this` which has been down-cast to `MetadataSlab`.
  class MetadataSlab* InitMetadataSlab(PageId start_id, uint32_t n_pages);

  // Returns a pointer to `this` which has been down-cast to `SmallSlab`.
  class SmallSlab* InitSmallSlab(PageId start_id, uint32_t n_pages);

  // Returns a pointer to `this` which has been down-cast to `LargeSlab`.
  class LargeSlab* InitLargeSlab(PageId start_id, uint32_t n_pages);

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

  class MetadataSlab* ToMetadata();
  const class MetadataSlab* ToMetadata() const;

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
        } metadata;
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
  class MetadataSlab* ToMetadata() = delete;
  const class MetadataSlab* ToMetadata() const = delete;
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
  class MetadataSlab* ToMetadata() = delete;
  const class MetadataSlab* ToMetadata() const = delete;
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

class MetadataSlab : public AllocatedSlab {
 public:
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;
};

class SmallSlab : public AllocatedSlab {
 public:
  class MetadataSlab* ToMetadata() = delete;
  const class MetadataSlab* ToMetadata() const = delete;
  class LargeSlab* ToLarge() = delete;
  const class LargeSlab* ToLarge() const = delete;
};

class LargeSlab : public AllocatedSlab {
 public:
  class MetadataSlab* ToMetadata() = delete;
  const class MetadataSlab* ToMetadata() const = delete;
  class SmallSlab* ToSmall() = delete;
  const class SmallSlab* ToSmall() const = delete;
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
static_assert(sizeof(Slab) == sizeof(MetadataSlab),
              "Slab subtype sizes must be equal, MetadataSlab different.");
static_assert(sizeof(Slab) == sizeof(SmallSlab),
              "Slab subtype sizes must be equal, SmallSlab different.");
static_assert(sizeof(Slab) == sizeof(LargeSlab),
              "Slab subtype sizes must be equal, LargeSlab different.");

}  // namespace ckmalloc
