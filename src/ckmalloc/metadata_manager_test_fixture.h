#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class MetadataManagerTest : public SlabManagerTest {
 public:
  static constexpr size_t kNumPages = 64;

  class TestMetadataManager {
   public:
    using MetadataManagerT = MetadataManagerImpl<TestSlabMap, TestSlabManager>;

    TestMetadataManager(class MetadataManagerTest* test_fixture,
                        TestSlabMap* slab_map, TestSlabManager* slab_manager);

    MetadataManagerT& Underlying() {
      return metadata_manager_;
    }

    const MetadataManagerT& Underlying() const {
      return metadata_manager_;
    }

    void* Alloc(size_t size, size_t alignment = 1);

    Slab* NewSlabMeta();

    void FreeSlabMeta(Slab* slab);

   private:
    class MetadataManagerTest* test_fixture_;
    MetadataManagerT metadata_manager_;
  };

  MetadataManagerTest()
      : SlabManagerTest(kNumPages),
        metadata_manager_(this, &SlabMap(), &SlabManager()),
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
  absl::flat_hash_map<void*, size_t> allocated_blocks_;

  // Maps allocations to the magic numbers that they are filled with. This is
  // only done for allocations made directly through
  // `MetadataManagerTest::Alloc`. Other test fixtures which depend on this one
  // will make allocations through the `TestMetadataManager`, which does not
  // modify this map.
  absl::flat_hash_map<void*, uint64_t> block_magics_;
};

}  // namespace ckmalloc
