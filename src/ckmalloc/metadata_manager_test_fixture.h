#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class MetadataManagerTest : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  MetadataManagerTest()
      : heap_(kNumPages),
        slab_manager_(&heap_, &slab_map_),
        metadata_manager_(&slab_map_, &slab_manager_),
        rng_(2021, 5) {}

  TestHeap& Heap() {
    return heap_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_;
  }

  TestMetadataManager& MetadataManager() {
    return metadata_manager_;
  }

  absl::StatusOr<size_t> SlabMetaFreelistLength() const;

  absl::StatusOr<void*> Alloc(size_t size, size_t alignment = 1);

  static void FillMagic(void* block, size_t size, uint64_t magic);
  static absl::Status CheckMagic(void* block, size_t size, uint64_t magic);

  absl::Status ValidateHeap() override;

 private:
  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;
  TestMetadataManager metadata_manager_;

  util::Rng rng_;

  // Maps allocations to their sizes and the magic numbers that they are filled
  // with.
  absl::flat_hash_map<void*, std::pair<size_t, uint64_t>> allocated_blocks_;
};

}  // namespace ckmalloc
