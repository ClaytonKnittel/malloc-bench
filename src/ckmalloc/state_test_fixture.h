#pragma once

#include <memory>
#include <utility>

#include "absl/status/status.h"

#include "src/ckmalloc/metadata_manager_test_fixture.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/state.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

class StateFixture : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  using TestSlabManager = SlabManagerFixture::TestSlabManager;
  using TestMetadataManager = MetadataManagerFixture::TestMetadataManager;

  using TestState = StateImpl<TestSlabMap, SlabManagerFixture::TestSlabManager>;

  StateFixture(
      std::shared_ptr<TestHeap> heap, std::shared_ptr<TestSlabMap> slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<TestSlabManager> slab_manager,
      std::shared_ptr<MetadataManagerFixture> metadata_manager_test_fixture,
      std::shared_ptr<TestMetadataManager> metadata_manager)
      : heap_(std::move(heap)),
        slab_map_(std::move(slab_map)),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        slab_manager_(std::move(slab_manager)),
        metadata_manager_test_fixture_(
            std::move(metadata_manager_test_fixture)),
        metadata_manager_(std::move(metadata_manager)) {}

  StateFixture()
      : StateFixture(std::make_shared<TestHeap>(kNumPages),
                     std::make_shared<TestSlabMap>()) {}

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
                heap, slab_map, slab_manager_test_fixture, slab_manager)) {}

  StateFixture(
      std::shared_ptr<TestHeap> heap, std::shared_ptr<TestSlabMap> slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<TestSlabManager> slab_manager,
      const std::pair<std::shared_ptr<MetadataManagerFixture>,
                      std::shared_ptr<TestMetadataManager>>& meta_managers)
      : StateFixture(std::move(heap), std::move(slab_map),
                     std::move(slab_manager_test_fixture),
                     std::move(slab_manager), meta_managers.first,
                     meta_managers.second) {}

  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  std::shared_ptr<MetadataManagerFixture> metadata_manager_test_fixture_;
  std::shared_ptr<TestMetadataManager> metadata_manager_;
};

template <>
StateFixture::TestState* StateFixture::TestState::state_;

}  // namespace ckmalloc
