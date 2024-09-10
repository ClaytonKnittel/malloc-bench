#pragma once

#include <cstddef>
#include <memory>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"

#include "src/ckmalloc/ckmalloc.h"
#include "src/ckmalloc/large_allocator_test_fixture.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class TestCkMalloc {
 public:
  using CkMallocT = CkMalloc<TestSlabMap, TestSlabManager, TestSmallAllocator,
                             TestLargeAllocator>;

  TestCkMalloc(class MainAllocatorFixture* test_fixture, TestSlabMap* slab_map,
               TestSlabManager* slab_manager, TestSmallAllocator* small_alloc,
               TestLargeAllocator* large_alloc);

  CkMallocT& Underlying() {
    return instance_;
  }

  const CkMallocT& Underlying() const {
    return instance_;
  }

  void* Malloc(size_t size);

  void* Calloc(size_t nmemb, size_t size);

  void* Realloc(void* ptr, size_t size);

  void Free(void* ptr);

 private:
  class CkMallocFixture* test_fixture_;
  CkMallocT instance_;
};

class MainAllocatorFixture : public CkMallocTest {
  friend TestCkMalloc;

 public:
  static constexpr const char* kPrefix = "[MainAllocatorFixture]";

  static constexpr size_t kNumPages = 64;

  MainAllocatorFixture(
      std::shared_ptr<TestHeapFactory> heap_factory,
      const std::shared_ptr<TestSlabMap>& slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<SmallAllocatorFixture> small_allocator_test_fixture,
      std::shared_ptr<LargeAllocatorFixture> large_allocator_test_fixture)
      : heap_factory_(std::move(heap_factory)),
        slab_map_(slab_map),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        small_allocator_test_fixture_(std::move(small_allocator_test_fixture)),
        large_allocator_test_fixture_(std::move(large_allocator_test_fixture)),
        main_allocator_(std::make_shared<TestMainAllocator>(
            this, slab_map_.get(),
            slab_manager_test_fixture_->SlabManagerPtr().get(),
            small_allocator_test_fixture_->SmallAllocatorPtr().get(),
            large_allocator_test_fixture_->LargeAllocatorPtr().get())),
        rng_(53, 47) {}

  const char* TestPrefix() const override {
    return kPrefix;
  }

  TestHeapFactory& HeapFactory() {
    return *heap_factory_;
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_test_fixture_->SlabManager();
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

  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<SmallAllocatorFixture> small_allocator_test_fixture_;
  std::shared_ptr<LargeAllocatorFixture> large_allocator_test_fixture_;
  std::shared_ptr<TestMainAllocator> main_allocator_;

  util::Rng rng_;

  // A map from allocation pointers to a pair of their size and magic number,
  // respectively.
  absl::btree_map<void*, std::pair<size_t, uint64_t>> allocations_;
};

}  // namespace ckmalloc
