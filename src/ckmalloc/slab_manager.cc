#include "src/ckmalloc/slab_manager.h"

#include <cstdint>
#include <optional>
#include <unistd.h>

#include "src/ckmalloc/allocator.h"
#include "src/ckmalloc/slab_id.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

const size_t SlabManager::kSlabSize = []() {
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
}();

/* static */
SlabManager* SlabManager::InitializeWithEmptyAlloc(Allocator* alloc) {
  CK_ASSERT(alloc->AllocatedBytes() == 0);
  // Allocate a metadata slab and place ourselves at the beginning of it.
  void* heap_start = alloc->Alloc(kSlabSize);
  auto* slab_manager = new (heap_start) SlabManager(alloc);
}

std::optional<SlabId> SlabManager::Alloc(uint32_t n_pages) {}

SlabManager::SlabManager(Allocator* alloc) : alloc_(alloc) {}

}  // namespace ckmalloc
