#pragma once

#include <memory>
#include <utility>

#include "absl/status/status.h"

#include "src/ckmalloc/main_allocator_test_fixture.h"
#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class StateFixture : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  using TestSlabManager = SlabManagerFixture::TestSlabManager;
  using TestMetadataManager = MetadataManagerFixture::TestMetadataManager;
  using TestMainAllocator = MainAllocatorFixture::TestMainAllocator;

  class TestState {
   public:
    TestState(std::shared_ptr<TestSlabMap> slab_map,
              std::shared_ptr<TestSlabManager> slab_manager,
              std::shared_ptr<TestMetadataManager> metadata_manager,
              std::shared_ptr<TestMainAllocator> main_allocator)
        : slab_map_(std::move(slab_map)),
          slab_manager_(std::move(slab_manager)),
          metadata_manager_(std::move(metadata_manager)),
          main_allocator_(std::move(main_allocator)) {}

   private:
    std::shared_ptr<TestSlabMap> slab_map_;
    std::shared_ptr<TestSlabManager> slab_manager_;
    std::shared_ptr<TestMetadataManager> metadata_manager_;
    std::shared_ptr<TestMainAllocator> main_allocator_;
  };

  StateFixture(
      std::shared_ptr<TestHeap> heap, std::shared_ptr<TestSlabMap> slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<TestSlabManager> slab_manager,
      std::shared_ptr<MetadataManagerFixture> metadata_manager_test_fixture,
      std::shared_ptr<TestMetadataManager> metadata_manager,
      std::shared_ptr<MainAllocatorFixture> main_allocator_test_fixture,
      std::shared_ptr<TestMainAllocator> main_allocator)
      : heap_(std::move(heap)),
        slab_map_(std::move(slab_map)),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        slab_manager_(std::move(slab_manager)),
        metadata_manager_test_fixture_(
            std::move(metadata_manager_test_fixture)),
        metadata_manager_(std::move(metadata_manager)),
        main_allocator_test_fixture_(std::move(main_allocator_test_fixture)),
        main_allocator_(std::move(main_allocator)),
        state_(std::make_shared<TestState>(
            slab_map_, slab_manager_, metadata_manager_, main_allocator_)) {}

  StateFixture()
      : StateFixture(std::make_shared<TestHeap>(kNumPages),
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

  TestMainAllocator& MainAllocator() {
    return *main_allocator_;
  }

  absl::Status ValidateHeap() override;

 private:
  // Only used for initializing `TestSlabManager` via the default constructor,
  // which needs the heap and slab_map to have been defined already.
  StateFixture(const std::shared_ptr<TestHeap>& heap,
               const std::shared_ptr<TestSlabMap>& slab_map)
      : StateFixture(heap, slab_map,
                     SlabManagerFixture::InitializeTest(heap, slab_map)) {}

  // Only used for initializing `TestMetadataManager` via the default
  // constructor, which needs the slab_map and slab_manager to have been
  // defined already.
  StateFixture(const std::shared_ptr<TestHeap>& heap,
               const std::shared_ptr<TestSlabMap>& slab_map,
               const std::pair<std::shared_ptr<SlabManagerFixture>,
                               std::shared_ptr<TestSlabManager>>& slab_managers)
      : StateFixture(heap, slab_map, slab_managers.first,
                     slab_managers.second) {}

  StateFixture(
      const std::shared_ptr<TestHeap>& heap,
      const std::shared_ptr<TestSlabMap>& slab_map,
      const std::shared_ptr<SlabManagerFixture>& slab_manager_test_fixture,
      const std::shared_ptr<TestSlabManager>& slab_manager)
      : StateFixture(
            heap, slab_map, slab_manager_test_fixture, slab_manager,
            MetadataManagerFixture::InitializeTest(
                heap, slab_map, slab_manager_test_fixture, slab_manager),
            std::make_shared<MainAllocatorFixture>(
                heap, slab_map, slab_manager_test_fixture, slab_manager)) {}

  StateFixture(
      std::shared_ptr<TestHeap> heap,
      const std::shared_ptr<TestSlabMap>& slab_map,
      const std::shared_ptr<SlabManagerFixture>& slab_manager_test_fixture,
      const std::shared_ptr<TestSlabManager>& slab_manager,
      const std::pair<std::shared_ptr<MetadataManagerFixture>,
                      std::shared_ptr<TestMetadataManager>>& meta_managers,
      const std::shared_ptr<MainAllocatorFixture>& main_allocator_test_fixture)
      : StateFixture(std::move(heap), slab_map,
                     std::move(slab_manager_test_fixture), slab_manager,
                     meta_managers.first, meta_managers.second,
                     main_allocator_test_fixture,
                     std::make_shared<TestMainAllocator>(
                         main_allocator_test_fixture.get(), slab_map.get(),
                         slab_manager.get())) {}

  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  std::shared_ptr<MetadataManagerFixture> metadata_manager_test_fixture_;
  std::shared_ptr<TestMetadataManager> metadata_manager_;
  std::shared_ptr<MainAllocatorFixture> main_allocator_test_fixture_;
  std::shared_ptr<TestMainAllocator> main_allocator_;
  std::shared_ptr<TestState> state_;
};

}  // namespace ckmalloc
