#include "src/ckmalloc/slab.h"

#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

void Slab::InitFreeSlab(SlabId start_id, uint32_t n_pages) {
  type_ = SlabType::kFree;
  mapped = {
    .id_ = start_id,
    .free = {
      .n_pages_ = n_pages,
    },
  };
}

SlabId Slab::StartId() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.id_;
}

uint32_t Slab::Pages() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  switch (type_) {
    case SlabType::kFree: {
      return mapped.free.n_pages_;
    }
    case SlabType::kLarge: {
      return mapped.large.n_pages_;
    }
    case SlabType::kMetadata:
    case SlabType::kSmall: {
      return 1;
    }
    case SlabType::kUnmapped: {
      CK_UNREACHABLE();
    }
  }
}

}  // namespace ckmalloc
