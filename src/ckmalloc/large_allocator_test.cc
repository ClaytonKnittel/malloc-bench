#include <cstddef>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "util/absl_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/large_allocator_test_fixture.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class LargeAllocatorTest : public ::testing::Test {
 public:
  static constexpr size_t kNumPages = 64;

  LargeAllocatorTest()
      : heap_factory_(std::make_shared<TestHeapFactory>(kNumPages * kPageSize)),
        slab_map_(std::make_shared<TestSlabMap>()),
        slab_manager_fixture_(std::make_shared<SlabManagerFixture>(
            heap_factory_, slab_map_, /*heap_idx=*/0)),
        large_allocator_fixture_(std::make_shared<LargeAllocatorFixture>(
            heap_factory_, slab_map_, slab_manager_fixture_)) {}

  TestHeapFactory& HeapFactory() {
    return slab_manager_fixture_->HeapFactory();
  }

  bench::Heap& Heap() {
    return *HeapFactory().Instance(0);
  }

  TestSlabMap& SlabMap() {
    return slab_manager_fixture_->SlabMap();
  }

  TestSlabManager& SlabManager() {
    return slab_manager_fixture_->SlabManager();
  }

  TestLargeAllocator& LargeAllocator() {
    return large_allocator_fixture_->LargeAllocator();
  }

  void Free(void* ptr) {
    LargeSlab* slab =
        SlabMap().FindSlab(SlabManager().PageIdFromPtr(ptr))->ToLarge();
    LargeAllocator().FreeLarge(slab, ptr);
  }

  absl::Status ValidateHeap() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateHeap());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateHeap());
    return absl::OkStatus();
  }

  absl::Status ValidateEmpty() {
    RETURN_IF_ERROR(slab_manager_fixture_->ValidateEmpty());
    RETURN_IF_ERROR(large_allocator_fixture_->ValidateEmpty());
    return absl::OkStatus();
  }

 private:
  std::shared_ptr<TestHeapFactory> heap_factory_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_fixture_;
  std::shared_ptr<LargeAllocatorFixture> large_allocator_fixture_;
};

}  // namespace ckmalloc
