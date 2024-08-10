#pragma once

#include <cstddef>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class SlabManagerTest : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  SlabManagerTest()
      : heap_(kNumPages), slab_manager_(&heap_, &slab_map_), rng_(1027, 3) {}

  TestHeap& Heap() {
    return heap_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_;
  }

  PageId HeapEnd() const {
    return PageId(heap_.Size() / kPageSize);
  }

  absl::Status ValidateHeap() override;

  absl::StatusOr<Slab*> AllocateSlab(uint32_t n_pages);

  absl::Status FreeSlab(Slab* slab);

 private:
  void FillMagic(Slab* slab, uint64_t magic);

  absl::Status CheckMagic(Slab* slab, uint64_t magic);

  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;
  util::Rng rng_;

  // Maps allocated slabs to a copy of their metadata and the magic number
  // copied into the whole slab.
  absl::flat_hash_map<Slab*, std::pair<Slab, uint64_t>> allocated_slabs_;
};

}  // namespace ckmalloc
