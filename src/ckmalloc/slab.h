#pragma once

#include <cstddef>

namespace ckmalloc {

enum class SlabType {
  kSmallBlocks,
  kLargeBlocks,
};

class Slab {
 public:
  static Slab* MakeSmallBlocksSlab();

 private:
  void* slab_start_;
  size_t n_pages_;
};

}  // namespace ckmalloc
