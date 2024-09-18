#pragma once

#include <memory>

#include "absl/status/status.h"

#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using TestSmallAllocator = SmallAllocatorImpl<TestSlabMap, TestSlabManager>;

class SmallAllocatorFixture : public CkMallocTest {
 public:
  static constexpr const char* kPrefix = "[SmallAllocatorFixture]";

  SmallAllocatorFixture(
      std::shared_ptr<TestHeapFactory> heap_factory,
      const std::shared_ptr<TestSlabMap>& slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<Freelist> freelist)
      : heap_factory_(std::move(heap_factory)),
        slab_map_(std::move(slab_map)),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        slab_manager_(slab_manager_test_fixture_->SlabManagerPtr()),
        freelist_(std::move(freelist)),
        small_allocator_(std::make_shared<TestSmallAllocator>(
            slab_map_.get(), slab_manager_.get(), freelist_.get())) {}

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
    return *slab_manager_;
  }

  TestSmallAllocator& SmallAllocator() {
    return *small_allocator_;
  }

  std::shared_ptr<TestSmallAllocator> SmallAllocatorPtr() {
    return small_allocator_;
  }

  absl::Status ValidateHeap() override;

  absl::Status ValidateEmpty();

 private:
  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  std::shared_ptr<Freelist> freelist_;
  std::shared_ptr<TestSmallAllocator> small_allocator_;
};

}  // namespace ckmalloc
