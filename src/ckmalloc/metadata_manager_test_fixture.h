#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class MetadataManagerTest : public SlabManagerTest {
 public:
  static constexpr size_t kNumPages = 64;

  MetadataManagerTest()
      : SlabManagerTest(kNumPages),
        metadata_manager_(&SlabMap(), &SlabManager()),
        rng_(2021, 5) {}

  TestMetadataManager& MetadataManager() {
    return metadata_manager_;
  }

  absl::StatusOr<size_t> SlabMetaFreelistLength() const;

  absl::StatusOr<void*> Alloc(size_t size, size_t alignment = 1);

  static void FillMagic(void* block, size_t size, uint64_t magic);
  static absl::Status CheckMagic(void* block, size_t size, uint64_t magic);

  absl::Status ValidateHeap() override;

 private:
  TestMetadataManager metadata_manager_;

  util::Rng rng_;

  // Maps allocations to their sizes and the magic numbers that they are filled
  // with.
  absl::flat_hash_map<void*, std::pair<size_t, uint64_t>> allocated_blocks_;
};

}  // namespace ckmalloc
