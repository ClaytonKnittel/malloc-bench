#pragma once

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class TestMetadataAlloc : public ckmalloc::TestMetadataAllocInterface {
 public:
  explicit TestMetadataAlloc(class TestMetadataManager* manager)
      : manager_(manager) {}

  ckmalloc::Slab* SlabAlloc() override;
  void SlabFree(ckmalloc::Slab* slab) override;
  void* Alloc(size_t size, size_t alignment) override;

  void ClearAllAllocs() override {}

 private:
  class TestMetadataManager* const manager_;
};

class TestMetadataManager {
 public:
  using MetadataManagerT =
      MetadataManagerImpl<TestGlobalMetadataAlloc, TestSlabMap>;

  TestMetadataManager(class MetadataManagerFixture* test_fixture,
                      TestHeapFactory* heap_factory, TestSlabMap* slab_map,
                      size_t heap_idx);

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

class MetadataManagerFixture : public CkMallocTest {
  friend TestMetadataManager;

 public:
  static constexpr const char* kPrefix = "[MetadataManagerFixture]";

  MetadataManagerFixture(std::shared_ptr<TestHeapFactory> heap_factory,
                         std::shared_ptr<TestSlabMap> slab_map, size_t heap_idx)
      : heap_factory_(std::move(heap_factory)),
        slab_map_(std::move(slab_map)),
        metadata_manager_(std::make_shared<TestMetadataManager>(
            this, heap_factory_.get(), slab_map_.get(), heap_idx)),
        allocator_(metadata_manager_.get()),
        rng_(2021, 5) {
    TestGlobalMetadataAlloc::OverrideAllocator(&allocator_);
  }

  ~MetadataManagerFixture() override {
    TestGlobalMetadataAlloc::ClearAllocatorOverride();
  }

  const char* TestPrefix() const override {
    return kPrefix;
  }

  TestHeapFactory& HeapFactory() {
    return *heap_factory_;
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  TestMetadataManager& MetadataManager() {
    return *metadata_manager_;
  }

  const TestMetadataManager& MetadataManager() const {
    return *metadata_manager_;
  }

  std::shared_ptr<TestMetadataManager> MetadataManagerPtr() const {
    return metadata_manager_;
  }

  absl::StatusOr<size_t> SlabMetaFreelistLength() const;

  absl::StatusOr<void*> Alloc(size_t size, size_t alignment = 1);

  absl::StatusOr<Slab*> NewSlabMeta();

  absl::Status FreeSlabMeta(Slab* slab);

  static void FillMagic(void* block, size_t size, uint64_t magic);
  absl::Status CheckMagic(void* block, size_t size, uint64_t magic);

  absl::Status ValidateHeap() override;

 private:
  // Validates a newly-allocated block, and writes over its data with magic
  // bytes.
  absl::Status TraceBlockAllocation(void* block, size_t size, size_t alignment);

  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<TestMetadataManager> metadata_manager_;

  TestMetadataAlloc allocator_;

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
