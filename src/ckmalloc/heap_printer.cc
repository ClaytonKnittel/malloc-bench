#include "src/ckmalloc/heap_printer.h"

#include <string>

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_map.h"
#include "src/ckmalloc/util.h"
#include "src/heap_interface.h"

namespace ckmalloc {

HeapPrinter::HeapPrinter(const bench::Heap* heap, const SlabMap* slab_map)
    : heap_(heap), slab_map_(slab_map) {}

std::string HeapPrinter::Print() {
  for (PageId page_id = PageId::Zero();
       page_id < PageId(heap_->Size() / kPageSize);) {
    Slab* slab = slab_map_->FindSlab(page_id);
    if (slab == nullptr) {
      // Assume this is a metadata slab.
      // TODO: print metadata slab.
      ++page_id;
      continue;
    }

    switch (slab->Type()) {
      case SlabType::kUnmapped: {
        CK_ASSERT_TRUE(false);
      }
      case SlabType::kFree: {
        FreeSlab* free_slab = slab->ToFree();
        break;
      }
      case SlabType::kSmall: {
        SmallSlab* small_slab = slab->ToSmall();
        break;
      }
      case SlabType::kLarge: {
        LargeSlab* large_slab = slab->ToLarge();
        break;
      }
    }
  }

  return "";
}

std::string HeapPrinter::PrintMetadata(PageId page_id) {}

std::string HeapPrinter::PrintFree(const FreeSlab* slab) {}

std::string HeapPrinter::PrintSmall(const SmallSlab* slab) {}

std::string HeapPrinter::PrintLarge(const LargeSlab* slab) {}

}  // namespace ckmalloc
