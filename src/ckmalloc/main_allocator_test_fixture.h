#pragma once

#include <cstddef>
#include <memory>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"

#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/main_allocator.h"
#include "src/ckmalloc/slab_manager_test_fixture.h"
#include "src/ckmalloc/small_allocator_test_fixture.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

class MainAllocatorFixture : public CkMallocTest {
  using TestSlabManager = SlabManagerFixture::TestSlabManager;
  using TestSmallAllocator = SmallAllocatorFixture::TestSmallAllocator;

 public:
  static constexpr const char* kPrefix = "[MainAllocatorFixture]";

  static constexpr size_t kNumPages = 64;

  class TestMainAllocator {
   public:
    using MainAllocatorT =
        MainAllocatorImpl<TestSlabMap, TestSlabManager, TestSmallAllocator>;

    TestMainAllocator(class MainAllocatorFixture* test_fixture,
                      TestSlabMap* slab_map, TestSlabManager* slab_manager,
                      TestSmallAllocator* small_alloc);

    MainAllocatorT& Underlying() {
      return main_allocator_;
    }

    const MainAllocatorT& Underlying() const {
      return main_allocator_;
    }

    Freelist& Freelist() {
      return main_allocator_.large_alloc_.freelist_;
    }

    void* Alloc(size_t user_size);

    void* Realloc(void* ptr, size_t user_size);

    void Free(void* ptr);

   private:
    class MainAllocatorFixture* test_fixture_;
    MainAllocatorT main_allocator_;
  };

  MainAllocatorFixture(
      std::shared_ptr<TestHeap> heap,
      const std::shared_ptr<TestSlabMap>& slab_map,
      std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture,
      std::shared_ptr<SmallAllocatorFixture> small_allocator_test_fixture)
      : heap_(std::move(heap)),
        slab_map_(slab_map),
        slab_manager_test_fixture_(std::move(slab_manager_test_fixture)),
        slab_manager_(slab_manager_test_fixture_->SlabManagerPtr()),
        small_allocator_test_fixture_(std::move(small_allocator_test_fixture)),
        small_allocator_(small_allocator_test_fixture_->SmallAllocatorPtr()),
        main_allocator_(std::make_shared<TestMainAllocator>(
            this, slab_map_.get(), slab_manager_.get(),
            small_allocator_.get())),
        rng_(53, 47) {}

  const char* TestPrefix() const override {
    return kPrefix;
  }

  TestHeap& Heap() {
    return *heap_;
  }

  TestSlabMap& SlabMap() {
    return *slab_map_;
  }

  TestSlabManager& SlabManager() {
    return *slab_manager_;
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

 private:
  static void FillMagic(void* allocation, size_t size, uint64_t magic);

  absl::Status CheckMagic(void* allocation, size_t size, uint64_t magic);

  std::shared_ptr<TestHeap> heap_;
  std::shared_ptr<TestSlabMap> slab_map_;
  std::shared_ptr<SlabManagerFixture> slab_manager_test_fixture_;
  std::shared_ptr<TestSlabManager> slab_manager_;
  std::shared_ptr<SmallAllocatorFixture> small_allocator_test_fixture_;
  std::shared_ptr<TestSmallAllocator> small_allocator_;
  std::shared_ptr<TestMainAllocator> main_allocator_;

  util::Rng rng_;

  // A map from allocation pointers to a pair of their size and magic number,
  // respectively.
  absl::btree_map<void*, std::pair<size_t, uint64_t>> allocations_;
};

}  // namespace ckmalloc
