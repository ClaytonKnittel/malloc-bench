#pragma once

#include <memory>

#include "absl/status/status.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/large_allocator.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using TestLargeAllocator = LargeAllocatorImpl<TestSlabMap, TestSlabManager>;

class LargeAllocatorFixture : public CkMallocTest {
 public:
  static constexpr const char* kPrefix = "[LargeAllocatorFixture]";

  LargeAllocatorFixture(
      const std::shared_ptr<TestSlabMap>& slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<Freelist> freelist)
      : slab_map_(std::move(slab_map)),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        slab_manager_(slab_manager_test_fixture_->SlabManagerPtr()),
        freelist_(std::move(freelist)),
        large_allocator_(std::make_shared<TestLargeAllocator>(
            slab_map_.get(), slab_manager_.get(), freelist_.get())) {}

  const char* TestPrefix() const override {
    return kPrefix;
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  TestSlabManager& SlabManager() {
    return *slab_manager_;
  }

  TestLargeAllocator& LargeAllocator() {
    return *large_allocator_;
  }

  std::shared_ptr<TestLargeAllocator> LargeAllocatorPtr() {
    return large_allocator_;
  }

  Freelist& Freelist() {
    return *freelist_;
  }

  const class Freelist& Freelist() const {
    return *freelist_;
  }

  std::vector<const TrackedBlock*> FreelistList() const {
    std::vector<const TrackedBlock*> tracked_blocks;
    for (const auto& exact_size_bin : Freelist().exact_size_bins_) {
      std::transform(exact_size_bin.begin(), exact_size_bin.end(),
                     std::back_inserter(tracked_blocks),
                     [](const TrackedBlock& block) { return &block; });
    }
    std::transform(Freelist().large_blocks_tree_.begin(),
                   Freelist().large_blocks_tree_.end(),
                   std::back_inserter(tracked_blocks),
                   [](const TrackedBlock& block) { return &block; });
    return tracked_blocks;
  }

  size_t FreelistSize() const {
    return FreelistList().size();
  }

  absl::Status ValidateHeap() override;

  static absl::Status ValidateEmpty();

 private:
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  std::shared_ptr<class Freelist> freelist_;
  std::shared_ptr<TestLargeAllocator> large_allocator_;
};

}  // namespace ckmalloc
