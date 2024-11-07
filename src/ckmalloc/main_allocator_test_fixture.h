#pragma once

#include <cstddef>
#include <memory>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/large_allocator_test_fixture.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class TestMainAllocator {
 public:
  using MainAllocatorT =
      MainAllocatorImpl<TestGlobalMetadataAlloc, TestSlabMap,
                        TestSmallAllocator, TestLargeAllocator>;

  TestMainAllocator(class MainAllocatorFixture* test_fixture,
                    TestSlabMap* slab_map, TestSmallAllocator* small_alloc,
                    TestLargeAllocator* large_alloc);

  MainAllocatorT& Underlying() {
    return main_allocator_;
  }

  const MainAllocatorT& Underlying() const {
    return main_allocator_;
  }

  Freelist& Freelist();

  Void* Alloc(size_t user_size);

  Void* AlignedAlloc(size_t user_size, size_t alignment);

  Void* Realloc(Void* ptr, size_t user_size);

  void Free(Void* ptr);

  size_t AllocSize(Void* ptr);

 private:
  void HandleAllocation(Void* alloc, size_t user_size);

  class MainAllocatorFixture* test_fixture_;
  MainAllocatorT main_allocator_;
};

class MainAllocatorFixture : public CkMallocTest {
  friend TestMainAllocator;

 public:
  static constexpr const char* kPrefix = "[MainAllocatorFixture]";

  MainAllocatorFixture(
      const std::shared_ptr<TestSlabMap>& slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<MetadataManagerFixture> metadata_manager_test_fixture,
      std::shared_ptr<SmallAllocatorFixture> small_allocator_test_fixture,
      std::shared_ptr<LargeAllocatorFixture> large_allocator_test_fixture)
      : slab_map_(slab_map),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        metadata_manager_test_fixture_(
            std::move(metadata_manager_test_fixture)),
        small_allocator_test_fixture_(std::move(small_allocator_test_fixture)),
        large_allocator_test_fixture_(std::move(large_allocator_test_fixture)),
        main_allocator_(std::make_shared<TestMainAllocator>(
            this, slab_map_.get(),
            small_allocator_test_fixture_->SmallAllocatorPtr().get(),
            large_allocator_test_fixture_->LargeAllocatorPtr().get())) {}

  const char* TestPrefix() const override {
    return kPrefix;
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_test_fixture_->SlabManager();
  }

  Freelist& Freelist() {
    return large_allocator_test_fixture_->Freelist();
  }

  TestMainAllocator& MainAllocator() {
    return *main_allocator_;
  }

  const TestMainAllocator& MainAllocator() const {
    return *main_allocator_;
  }

  std::shared_ptr<TestMainAllocator> MainAllocatorPtr() const {
    return main_allocator_;
  }

  absl::Status ValidateHeap() override;

  absl::Status ValidateEmpty();

 private:
  static void FillMagic(void* allocation, size_t size, uint64_t magic);

  absl::Status CheckMagic(void* allocation, size_t size, uint64_t magic);

  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<MetadataManagerFixture> metadata_manager_test_fixture_;
  std::shared_ptr<SmallAllocatorFixture> small_allocator_test_fixture_;
  std::shared_ptr<LargeAllocatorFixture> large_allocator_test_fixture_;
  std::shared_ptr<TestMainAllocator> main_allocator_;

  // A map from allocation pointers to a pair of their size and magic number,
  // respectively.
  absl::btree_map<Void*, std::pair<size_t, uint64_t>> allocations_;

  // A set of all known-to-be-allocated mmap-block heaps.
  absl::flat_hash_set<Void*> mmap_blocks_;
};

}  // namespace ckmalloc
