#pragma once

#include <string>

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class HeapPrinter {
 public:
  HeapPrinter(const bench::Heap* heap, const SlabMap* slab_map);

  std::string Print();

 private:
  std::string PrintMetadata(PageId page_id);

  std::string PrintFree(const FreeSlab* slab);

  std::string PrintSmall(const SmallSlab* slab);

  std::string PrintLarge(const LargeSlab* slab);

  const bench::Heap* const heap_;
  const SlabMap* const slab_map_;
};

}  // namespace ckmalloc
