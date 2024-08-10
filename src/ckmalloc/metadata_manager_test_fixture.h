#pragma once

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class MetadataManagerFixture : public CkMallocTest {
  using TestSlabManager = SlabManagerFixture::TestSlabManager;

 public:
  static constexpr size_t kNumPages = 64;

  class TestMetadataManager {
   public:
    using MetadataManagerT = MetadataManagerImpl<TestSlabMap, TestSlabManager>;

    TestMetadataManager(class MetadataManagerFixture* test_fixture,
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
    class MetadataManagerFixture* test_fixture_;
    MetadataManagerT metadata_manager_;
  };

  MetadataManagerFixture(
      std::shared_ptr<TestHeap> heap, std::shared_ptr<TestSlabMap> slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<TestSlabManager> slab_manager,
      std::shared_ptr<TestMetadataManager> metadata_manager)
      : heap_(std::move(heap)),
        slab_map_(std::move(slab_map)),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        slab_manager_(std::move(slab_manager)),
        metadata_manager_(std::move(metadata_manager)),
        rng_(2021, 5) {}

  MetadataManagerFixture()
      : MetadataManagerFixture(std::make_shared<TestHeap>(kNumPages),
                               std::make_shared<TestSlabMap>()) {}

  TestHeap& Heap() {
    return *heap_;
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  TestSlabManager& SlabManager() {
    return *slab_manager_;
  }

  TestMetadataManager& MetadataManager() {
    return *metadata_manager_;
  }

  const TestMetadataManager& MetadataManager() const {
    return *metadata_manager_;
  }

  absl::StatusOr<size_t> SlabMetaFreelistLength() const;

  absl::StatusOr<void*> Alloc(size_t size, size_t alignment = 1);

  absl::StatusOr<Slab*> NewSlabMeta();

  absl::Status FreeSlabMeta(Slab* slab);

  static void FillMagic(void* block, size_t size, uint64_t magic);
  static absl::Status CheckMagic(void* block, size_t size, uint64_t magic);

  absl::Status ValidateHeap() override;

 private:
  // Only used for initializing `TestSlabManager` via the default constructor,
  // which needs the heap and slab_map to have been defined already.
  MetadataManagerFixture(const std::shared_ptr<TestHeap>& heap,
                         const std::shared_ptr<TestSlabMap>& slab_map)
      : MetadataManagerFixture(
            heap, slab_map,
            SlabManagerFixture::InitializeTest(heap, slab_map)) {}

  // Only used for initializing `TestMetadataManager` via the default
  // constructor, which needs the slab_map and slab_manager to have been
  // defined already.
  MetadataManagerFixture(
      const std::shared_ptr<TestHeap>& heap,
      const std::shared_ptr<TestSlabMap>& slab_map,
      const std::pair<std::shared_ptr<SlabManagerFixture>,
                      std::shared_ptr<TestSlabManager>>& slab_managers)
      : MetadataManagerFixture(
            heap, slab_map, slab_managers.first, slab_managers.second,
            std::make_shared<TestMetadataManager>(
                this, slab_map.get(), slab_managers.second.get())) {}

  // Validates a newly-allocated block, and writes over its data with magic
  // bytes.
  absl::Status TraceBlockAllocation(void* block, size_t size, size_t alignment);

  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  std::shared_ptr<TestMetadataManager> metadata_manager_;

  util::Rng rng_;

  // Maps allocations to their sizes and the magic numbers that they are filled
  // with.
  absl::flat_hash_map<void*, size_t> allocated_blocks_;

  // A set of all the freed slab metadata.
  absl::flat_hash_set<Slab*> freed_slab_metadata_;

  // Maps allocations to the magic numbers that they are filled with. This is
  // only done for allocations made directly through
  // `MetadataManagerTest::Alloc`. Other test fixtures which depend on this one
  // will make allocations through the `TestMetadataManager`, which does not
  // modify this map.
  absl::flat_hash_map<void*, uint64_t> block_magics_;
};

}  // namespace ckmalloc
