#pragma once

#include <cstdint>

#include "src/ckmalloc/red_black_tree.h"
#include "src/ckmalloc/slab_id.h"

namespace ckmalloc {

enum class SlabType {
  kMetadata,
  kSmall,
  kLarge,
};

// Slab metadata class, which is stored separately from the slab it describes,
// in a metadata slab.
class Slab {
 public:
  Slab(SlabId slab_id, uint32_t n_pages, SlabType type)
      : id_(slab_id), n_pages_(n_pages), type_(type) {}

  uint32_t Pages() const {
    return n_pages_;
  }

  bool operator<(const Slab& slab) const {
    return n_pages_ < slab.n_pages_;
  }

 protected:
 private:
  class FreeMultiPage : public RbNode {};

  SlabId id_;
  uint32_t n_pages_;

  SlabType type_;

  union {};
};

}  // namespace ckmalloc
