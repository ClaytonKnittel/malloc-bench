#pragma once

#include "src/ckmalloc/red_black_tree.h"

namespace ckmalloc {

class FreeSinglePageSlab {
 public:
  FreeSinglePageSlab* NextFree() {
    return next_free_;
  }

  const FreeSinglePageSlab* NextFree() const {
    return next_free_;
  }

  FreeSinglePageSlab* PrevFree() {
    return prev_free_;
  }

  const FreeSinglePageSlab* PrevFree() const {
    return prev_free_;
  }

  void SetNextFree(FreeSinglePageSlab* next_free) {
    next_free_ = next_free;
  }

 private:
  // For free single-page slabs, we have a doubly-linked list of free slabs.
  FreeSinglePageSlab* next_free_;
  FreeSinglePageSlab* prev_free_;
};

// Free multi-page slabs are kept in a red-black tree in sorted order by size.
class FreeMultiPageSlab : public RbNode {
 public:
  explicit FreeMultiPageSlab(uint32_t n_pages) : n_pages_(n_pages) {}

  bool operator<(const FreeMultiPageSlab& slab) const {
    return n_pages_ < slab.n_pages_;
  }

  uint32_t Pages() const {
    return n_pages_;
  }

 private:
  // The size of the free slab.
  uint32_t n_pages_;
};

}  // namespace ckmalloc
