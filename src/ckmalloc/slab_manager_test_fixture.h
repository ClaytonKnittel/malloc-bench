#pragma once

#include <cstddef>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class SlabManagerFixture : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  class TestSlabManager {
   public:
    using SlabManagerT = SlabManagerImpl<TestGlobalMetadataAlloc, TestSlabMap>;

    TestSlabManager(class SlabManagerFixture* test_fixture, TestHeap* heap,
                    TestSlabMap* slab_map);

    SlabManagerT& Underlying() {
      return slab_manager_;
    }

    void* PageStartFromId(PageId page_id) const;

    PageId PageIdFromPtr(const void* ptr) const;

    template <typename S, typename... Args>
    std::optional<std::pair<PageId, S*>> Alloc(uint32_t n_pages, Args...);

    void Free(AllocatedSlab* slab);

   private:
    void HandleAlloc(AllocatedSlab* slab);

    class SlabManagerFixture* test_fixture_;
    SlabManagerT slab_manager_;
  };

  SlabManagerFixture(std::shared_ptr<TestHeap> heap,
                     std::shared_ptr<TestSlabMap> slab_map,
                     std::shared_ptr<TestSlabManager> slab_manager)
      : heap_(std::move(heap)),
        slab_map_(std::move(slab_map)),
        slab_manager_(std::move(slab_manager)),
        rng_(1027, 3) {}

  SlabManagerFixture()
      : SlabManagerFixture(std::make_shared<TestHeap>(kNumPages),
                           std::make_shared<TestSlabMap>()) {}

  static std::pair<std::shared_ptr<SlabManagerFixture>,
                   std::shared_ptr<TestSlabManager>>
  InitializeTest(const std::shared_ptr<TestHeap>& heap,
                 const std::shared_ptr<TestSlabMap>& slab_map);

  std::shared_ptr<TestHeap> HeapPtr() {
    return heap_;
  }

  TestHeap& Heap() {
    return *heap_;
  }

  std::shared_ptr<TestSlabMap> SlabMapPtr() {
    return slab_map_;
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  std::shared_ptr<TestSlabManager>& SlabManagerPtr() {
    return slab_manager_;
  }

  TestSlabManager& SlabManager() {
    return *slab_manager_;
  }

  PageId HeapEnd() const {
    return PageId(heap_->Size() / kPageSize);
  }

  absl::Status ValidateHeap() override;

  absl::StatusOr<AllocatedSlab*> AllocateSlab(uint32_t n_pages);

  absl::Status FreeSlab(AllocatedSlab* slab);

 private:
  // Only used for initializing `TestSlabManager` via the default constructor,
  // which needs the heap and slab_map to have been defined already.
  SlabManagerFixture(const std::shared_ptr<TestHeap>& heap,
                     const std::shared_ptr<TestSlabMap>& slab_map)
      : SlabManagerFixture(heap, slab_map,
                           std::make_shared<TestSlabManager>(this, heap.get(),
                                                             slab_map.get())) {}

  void FillMagic(AllocatedSlab* slab, uint64_t magic);

  absl::Status CheckMagic(AllocatedSlab* slab, uint64_t magic);

  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  util::Rng rng_;

  // Maps allocated slabs to a copy of their metadata.
  absl::flat_hash_map<AllocatedSlab*, AllocatedSlab> allocated_slabs_;

  // Maps allocated slabs to a copy of their magic number which is copied into
  // the whole slab. This is only used when slabs are allocated through the
  // `SlabManagerTestFixture` interface, since other tests that use this fixture
  // will write to the allocated slabs.
  absl::flat_hash_map<AllocatedSlab*, uint64_t> slab_magics_;
};

template <typename S, typename... Args>
std::optional<std::pair<PageId, S*>> SlabManagerFixture::TestSlabManager::Alloc(
    uint32_t n_pages, Args... args) {
  using AllocResult = std::pair<PageId, S*>;
  DEFINE_OR_RETURN_OPT(
      AllocResult, result,
      slab_manager_.template Alloc<S>(n_pages, std::forward<Args>(args)...));
  HandleAlloc(result.second);
  return result;
}

}  // namespace ckmalloc
