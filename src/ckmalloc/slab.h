#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ckmalloc/slab_id.h"

namespace ckmalloc {

static constexpr uint32_t kPageShift = 12;
// The size of slabs in bytes.
static constexpr size_t kPageSize = 1 << kPageShift;

enum class SlabType {
  // The slab metadata is free and in the metadata freelist. It is not managing
  // any allocated slab and can be claimed for a new slab.
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
 public:
  // Returns the number of pages that this slab manages. This slab must not be a
  // freed slab metadata.
  uint32_t Pages() const;

 private:
  SlabType type_;

  union {
    struct {
      uint32_t n_pages_;
    } free;

    struct {
      SlabId id_;

      union {
        struct {
        } metadata;
        struct {
        } small;
        struct {
          uint32_t n_pages_;
        } large;
      };
    } allocated;
  };
};

}  // namespace ckmalloc
