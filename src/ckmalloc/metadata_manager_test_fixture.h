#pragma once

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class TestMetadataAlloc : public ckmalloc::TestMetadataAllocInterface {
 public:
  explicit TestMetadataAlloc(class TestMetadataManager* manager)
      : manager_(manager) {}

  ckmalloc::Slab* SlabAlloc() override;
  void SlabFree(ckmalloc::MappedSlab* slab) override;
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
                      TestHeap* heap, TestSlabMap* slab_map, size_t heap_size);

  MetadataManagerT& Underlying() {
    return metadata_manager_;
  }

  const MetadataManagerT& Underlying() const {
    return metadata_manager_;
  }

  void* Alloc(size_t size, size_t alignment = 1);

  Slab* NewSlabMeta();

  void FreeSlabMeta(MappedSlab* slab);

 private:
  class MetadataManagerFixture* test_fixture_;
  MetadataManagerT metadata_manager_;
};

class MetadataManagerFixture : public CkMallocTest {
  friend TestMetadataManager;

 public:
  static constexpr const char* kPrefix = "[MetadataManagerFixture]";

  MetadataManagerFixture(std::shared_ptr<TestHeap> heap,
                         std::shared_ptr<TestSlabMap> slab_map,
                         size_t heap_size)
      : heap_(std::move(heap)),
        slab_map_(std::move(slab_map)),
        metadata_manager_(std::make_shared<TestMetadataManager>(
            this, heap_.get(), slab_map_.get(), heap_size)),
        allocator_(metadata_manager_.get()) {
    TestGlobalMetadataAlloc::OverrideAllocator(&allocator_);
  }

  ~MetadataManagerFixture() override {
    TestGlobalMetadataAlloc::ClearAllocatorOverride();
  }

  const char* TestPrefix() const override {
    return kPrefix;
  }

  TestHeap& Heap() {
    return *heap_;
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

  const absl::flat_hash_set<Slab*>& AllocatedSlabMeta() const;

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

  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<TestMetadataManager> metadata_manager_;

  TestMetadataAlloc allocator_;

  // Maps allocations to their sizes and the magic numbers that they are filled
  // with.
  absl::flat_hash_map<void*, size_t> allocated_blocks_;

  // A set of all the allocated slab metadata.
  absl::flat_hash_set<Slab*> alloc_slab_metadata_;
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
