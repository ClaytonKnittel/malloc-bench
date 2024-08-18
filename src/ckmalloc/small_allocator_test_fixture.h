#pragma once

#include <memory>

#include "absl/status/status.h"

#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class SmallAllocatorFixture : public CkMallocTest {
  using TestSlabManager = SlabManagerFixture::TestSlabManager;

 public:
  using TestSmallAllocator = SmallAllocatorImpl<TestSlabMap, TestSlabManager>;

  SmallAllocatorFixture(
      std::shared_ptr<TestHeap> heap,
      const std::shared_ptr<TestSlabMap>& slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      const std::shared_ptr<TestSlabManager>& slab_manager)
      : heap_(std::move(heap)),
        slab_map_(std::move(slab_map)),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        slab_manager_(slab_manager),
        small_allocator_(std::make_shared<TestSmallAllocator>(
            slab_map_.get(), slab_manager_.get())) {}

  TestHeap& Heap() {
    return *heap_;
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

  absl::Status ValidateHeap() override;

 private:
  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  std::shared_ptr<TestSmallAllocator> small_allocator_;
};

}  // namespace ckmalloc