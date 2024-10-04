#pragma once

#include <memory>
#include <numeric>
#include <ranges>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/sys_alloc.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class TestSlabManager {
 public:
  using SlabManagerT = SlabManagerImpl<TestGlobalMetadataAlloc, TestSlabMap>;

  TestSlabManager(class SlabManagerFixture* test_fixture, TestSlabMap* slab_map,
                  size_t heap_size);

  SlabManagerT& Underlying() {
    return slab_manager_;
  }

  template <typename S, typename... Args>
  std::optional<std::pair<PageId, S*>> Alloc(uint32_t n_pages, Args...);

  bool Resize(AllocatedSlab* slab, uint32_t new_size);

  void Free(AllocatedSlab* slab);

  Block* FirstBlockInBlockedSlab(const BlockedSlab* slab) const;

 private:
  void HandleAlloc(AllocatedSlab* slab);

  class SlabManagerFixture* test_fixture_;
  SlabManagerT slab_manager_;
};

class SlabManagerFixture : public CkMallocTest {
  friend TestSlabManager;

 public:
  static constexpr const char* kPrefix = "[SlabManagerFixture]";

  static auto Heaps() {
    return *TestSysAlloc::Instance() |
           std::ranges::views::filter(
               [](const std::pair<void*, std::pair<HeapType, TestHeap*>>& it)
                   -> bool { return it.second.first == HeapType::kUserHeap; });
  }

  auto SlabsInHeap() {
    return Heaps() |
           std::ranges::views::transform([this](auto it) -> IterableTestHeap {
             return IterableTestHeap(it.second.second, &SlabMap());
           }) |
           std::ranges::views::join;
  }

  static size_t TotalHeapsSize() {
    auto heaps =
        Heaps() | std::ranges::views::transform([](const auto& it) -> size_t {
          return it.second.second->Size();
        });
    return std::accumulate(heaps.begin(), heaps.end(), static_cast<size_t>(0));
  }

  // Only used for initializing `TestSlabManager` via the default constructor,
  // which needs the heap and slab_map to have been defined already.
  explicit SlabManagerFixture(std::shared_ptr<TestHeapFactory> heap_factory,
                              std::shared_ptr<TestSlabMap> slab_map,
                              size_t heap_size)
      : heap_factory_(std::move(heap_factory)),
        slab_map_(std::move(slab_map)),
        slab_manager_(std::make_shared<TestSlabManager>(this, slab_map_.get(),
                                                        heap_size)),
        rng_(1027, 3) {}

  const char* TestPrefix() const override {
    return kPrefix;
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

  absl::Status ValidateHeap() override;

  // Checks that the heap is empty, returning a non-ok status if it isn't.
  absl::Status ValidateEmpty();

  absl::StatusOr<AllocatedSlab*> AllocateSlab(uint32_t n_pages);

  absl::Status FreeSlab(AllocatedSlab* slab);

 private:
  static void FillMagic(AllocatedSlab* slab, uint64_t magic);

  absl::Status CheckMagic(AllocatedSlab* slab, uint64_t magic);

  std::shared_ptr<TestHeapFactory> heap_factory_;
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
std::optional<std::pair<PageId, S*>> TestSlabManager::Alloc(uint32_t n_pages,
                                                            Args... args) {
  using AllocResult = std::pair<PageId, S*>;
  DEFINE_OR_RETURN_OPT(
      AllocResult, result,
      slab_manager_.template Alloc<S>(n_pages, std::forward<Args>(args)...));
  auto [page_id, slab] = result;

  // Allocated slabs must map every page to their metadata.
  HandleAlloc(slab);
  return std::make_pair(page_id, slab);
}

}  // namespace ckmalloc
