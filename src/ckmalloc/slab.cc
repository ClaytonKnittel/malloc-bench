#include "src/ckmalloc/slab.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

void Slab::InitUnmappedSlab(Slab* next_unmapped) {
  type_ = SlabType::kUnmapped;
  unmapped = {
    .next_unmapped_ = next_unmapped,
  };
}

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

void Slab::InitSmallSlab(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kSmall;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .small = {},
  };
}

void Slab::InitLargeSlab(PageId start_id, uint32_t n_pages) {
  type_ = SlabType::kLarge;
  mapped = {
    .id_ = start_id,
    .n_pages_ = n_pages,
    .large = {},
  };
}

Slab* Slab::NextUnmappedSlab() {
  CK_ASSERT(type_ == SlabType::kUnmapped);
  return unmapped.next_unmapped_;
}

void Slab::SetNextUnmappedSlab(Slab* next_unmapped) {
  CK_ASSERT(type_ == SlabType::kUnmapped);
  unmapped.next_unmapped_ = next_unmapped;
}

PageId Slab::StartId() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.id_;
}

PageId Slab::EndId() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.id_ + Pages() - 1;
}

uint32_t Slab::Pages() const {
  CK_ASSERT(type_ != SlabType::kUnmapped);
  return mapped.n_pages_;
}

}  // namespace ckmalloc
