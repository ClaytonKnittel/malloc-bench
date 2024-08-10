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
  void InitUnmappedSlab(Slab* next_unmapped = nullptr);

  void InitFreeSlab(PageId start_id, uint32_t n_pages);

  void InitMetadataSlab(PageId start_id, uint32_t n_pages);

  void InitSmallSlab(PageId start_id, uint32_t n_pages);

  void InitLargeSlab(PageId start_id, uint32_t n_pages);

  // Returns the next unmapped slab in the freelist.
  Slab* NextUnmappedSlab();

  // Returns the next unmapped slab in the freelist.
  const Slab* NextUnmappedSlab() const;

  void SetNextUnmappedSlab(Slab* next_unmapped);

  // Returns the `PageId` of the first page in this slab.
  PageId StartId() const;

  // Returns the `PageId` of the last page in this slab.
  PageId EndId() const;

  SlabType Type() const {
    return type_;
  }

  // Returns the number of pages that this slab manages. This slab must not be a
  // freed slab metadata.
  uint32_t Pages() const;

 private:
  Slab() {}

  SlabType type_;

  union {
    struct {
      // Unmapped slabs are held in a singly-linked freelist managed by the
      // metadata manager.
      Slab* next_unmapped_;
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
        } large;
      };
    } mapped;
  };
};

}  // namespace ckmalloc
