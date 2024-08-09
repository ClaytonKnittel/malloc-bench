#pragma once

#include "src/ckmalloc/linked_list.h"
#include "src/ckmalloc/red_black_tree.h"

namespace ckmalloc {

// For free single-page slabs, we have a doubly-linked list of free slabs.
class FreeSinglePageSlab : public LinkedListNode {};

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
