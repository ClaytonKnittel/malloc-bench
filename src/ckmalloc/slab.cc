#include "src/ckmalloc/slab.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

void Slab::InitFreeSlab(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kFree;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .free = {},
  };
}

void Slab::InitMetadataSlab(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kMetadata;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .metadata = {},
  };
}

PageId Slab::StartId() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.id_;
}

PageId Slab::EndId() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.id_ + Pages();
}

uint32_t Slab::Pages() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.n_pages_;
}

}  // namespace ckmalloc
