#pragma once

#include <memory>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/testlib.h"
#include "src/heap_interface.h"
#include "src/rng.h"

namespace ckmalloc {

class TestSlabManager {
 public:
  using SlabManagerT = SlabManagerImpl<TestGlobalMetadataAlloc, TestSlabMap>;

  TestSlabManager(class SlabManagerFixture* test_fixture, TestHeapFactory* heap,
                  TestSlabMap* slab_map, size_t heap_idx);

  SlabManagerT& Underlying() {
    return slab_manager_;
  }

  void* PageStartFromId(PageId page_id) const;

  PageId PageIdFromPtr(const void* ptr) const;

  template <typename S, typename... Args>
  std::optional<std::pair<PageId, S*>> Alloc(uint32_t n_pages, Args...);

  template <typename S, typename... Args>
  std::optional<std::tuple<S*, FreeSlab*, S*>> Carve(S* slab, uint32_t from,
                                                     uint32_t to, Args...);

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

  class HeapIterator {
    friend class SlabManagerFixture;

   public:
    bool operator==(HeapIterator other) const {
      return current_ == other.current_;
    }
    bool operator!=(HeapIterator other) const {
      return !(*this == other);
    }

    Slab* operator*() {
      Slab* slab = fixture_->SlabMap().FindSlab(current_);
      // Since the slab map may have stale entries, we need to check that the
      // slab we found still applies to this page.
      return slab != nullptr && slab->Type() != SlabType::kUnmapped &&
                     current_ >= slab->ToMapped()->StartId() &&
                     current_ <= slab->ToMapped()->EndId()
                 ? slab
                 : nullptr;
    }
    Slab* operator->() {
      return fixture_->SlabMap().FindSlab(current_);
    }

    HeapIterator operator++() {
      Slab* current = **this;
      // If current is `nullptr`, then this is a metadata slab.
      current_ += current != nullptr ? current->ToMapped()->Pages() : 1;
      return *this;
    }
    HeapIterator operator++(int) {
      HeapIterator copy = *this;
      ++*this;
      return copy;
    }

   private:
    explicit HeapIterator(SlabManagerFixture* fixture)
        : HeapIterator(fixture, PageId::Zero()) {}

    HeapIterator(SlabManagerFixture* fixture, PageId page_id)
        : fixture_(fixture), current_(page_id) {}

    SlabManagerFixture* const fixture_;
    PageId current_;
  };

  HeapIterator HeapBegin() {
    return HeapIterator(this);
  }

  HeapIterator HeapEnd() {
    return HeapIterator(this, HeapEndId());
  }

  // Only used for initializing `TestSlabManager` via the default constructor,
  // which needs the heap and slab_map to have been defined already.
  SlabManagerFixture(std::shared_ptr<TestHeapFactory> heap_factory,
                     std::shared_ptr<TestSlabMap> slab_map, size_t heap_idx)
      : heap_factory_(std::move(heap_factory)),
        slab_map_(std::move(slab_map)),
        slab_manager_(std::make_shared<TestSlabManager>(
            this, heap_factory_.get(), slab_map_.get(), heap_idx)),
        rng_(1027, 3) {}

  const char* TestPrefix() const override {
    return kPrefix;
  }

  TestHeapFactory& HeapFactory() {
    return *heap_factory_;
  }

  bench::Heap& SlabHeap() {
    return *heap_factory_->Instance(slab_manager_->Underlying().heap_idx_);
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

  PageId HeapEndId() const {
    return PageId(
        heap_factory_->Instance(slab_manager_->Underlying().heap_idx_)->Size() /
        kPageSize);
  }

  absl::Status ValidateHeap() override;

  // Checks that the heap is empty, returning a non-ok status if it isn't.
  absl::Status ValidateEmpty();

  absl::StatusOr<AllocatedSlab*> AllocateSlab(uint32_t n_pages);

  absl::Status FreeSlab(AllocatedSlab* slab);

 private:
  void FillMagic(AllocatedSlab* slab, uint64_t magic);

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
  return result;
}

template <typename S, typename... Args>
std::optional<std::tuple<S*, FreeSlab*, S*>> TestSlabManager::Carve(
    S* slab, uint32_t from, uint32_t to, Args... args) {
  using AllocResult = std::tuple<S*, FreeSlab*, S*>;
  DEFINE_OR_RETURN_OPT(AllocResult, result,
                       slab_manager_.template Carve<S>(
                           slab, from, to, std::forward<Args>(args)...));
  auto [left_slab, free_slab, right_slab] = result;

  auto it = test_fixture_->allocated_slabs_.find(slab);
  CK_ASSERT_TRUE(it != test_fixture_->allocated_slabs_.end());
  test_fixture_->allocated_slabs_.erase(it);

  if (left_slab != nullptr) {
    HandleAlloc(left_slab);
  }
  if (right_slab != nullptr) {
    HandleAlloc(right_slab);
  }
  return result;
}

}  // namespace ckmalloc
