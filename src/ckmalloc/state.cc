#include "src/ckmalloc/state.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"

namespace ckmalloc {

template <>
State* State::state_ = nullptr;

// template <>
// TestState* TestState::state_ = nullptr;

Slab* GlobalMetadataAlloc::SlabAlloc() {
  return State::Instance()->MetadataManager()->NewSlabMeta();
}

void GlobalMetadataAlloc::SlabFree(MappedSlab* slab) {
  State::Instance()->MetadataManager()->FreeSlabMeta(slab);
}

void* GlobalMetadataAlloc::Alloc(size_t size, size_t alignment) {
  return State::Instance()->MetadataManager()->Alloc(size, alignment);
}

}  // namespace ckmalloc
