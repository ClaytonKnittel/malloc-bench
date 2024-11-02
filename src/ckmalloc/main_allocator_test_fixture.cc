#include "src/ckmalloc/main_allocator_test_fixture.h"

#include <bit>
#include <cstddef>

#include "absl/status/status.h"
#include "folly/Random.h"
#include "util/absl_util.h"

#include "src/ckmalloc/block.h"
#include "src/ckmalloc/common.h"
#include "src/ckmalloc/freelist.h"
#include "src/ckmalloc/size_class.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"

namespace ckmalloc {

TestMainAllocator::TestMainAllocator(MainAllocatorFixture* test_fixture,
                                     TestSlabMap* slab_map,
                                     TestSlabManager* slab_manager,
                                     TestSmallAllocator* small_alloc,
                                     TestLargeAllocator* large_alloc)
    : test_fixture_(test_fixture),
      main_allocator_(slab_map, slab_manager, small_alloc, large_alloc) {}

Freelist& TestMainAllocator::Freelist() {
  return test_fixture_->Freelist();
}

Void* TestMainAllocator::Alloc(size_t user_size) {
  Void* alloc = main_allocator_.Alloc(user_size);
  if (alloc == nullptr) {
    return nullptr;
  }
  HandleAllocation(alloc, user_size);
  return alloc;
}

Void* TestMainAllocator::AlignedAlloc(size_t user_size, size_t alignment) {
  Void* alloc = main_allocator_.AlignedAlloc(user_size, alignment);
  if (alloc == nullptr) {
    return nullptr;
  }
  HandleAllocation(alloc, user_size);
  return alloc;
}

Void* TestMainAllocator::Realloc(Void* ptr, size_t user_size) {
  MappedSlab* slab = test_fixture_->SlabMap().FindSlab(PageId::FromPtr(ptr));
  CK_ASSERT_NE(slab, nullptr);
  if (slab->Type() == SlabType::kMmap) {
    size_t res = test_fixture_->mmap_blocks_.erase(ptr);
    CK_ASSERT_EQ(res, 1);
  }

  auto it = test_fixture_->allocations_.find(ptr);
  CK_ASSERT_FALSE(it == test_fixture_->allocations_.end());
  auto [old_size, magic] = it->second;

  Void* new_alloc = main_allocator_.Realloc(ptr, user_size);
  if (new_alloc == nullptr) {
    // Old memory block will not be freed, so no need to remove it from the
    // `allocations_` table.
    return nullptr;
  }

  if (user_size > old_size) {
    MainAllocatorFixture::FillMagic(PtrAdd(new_alloc, old_size),
                                    user_size - old_size,
                                    std::rotr(magic, (old_size % 8) * 8));
  }

  test_fixture_->allocations_.erase(it);

  auto [it2, inserted] =
      test_fixture_->allocations_.insert({ new_alloc, { user_size, magic } });
  CK_ASSERT_TRUE(inserted);

  slab = test_fixture_->SlabMap().FindSlab(PageId::FromPtr(new_alloc));
  CK_ASSERT_NE(slab, nullptr);
  if (slab->Type() == SlabType::kMmap) {
    auto [_, inserted] = test_fixture_->mmap_blocks_.insert(new_alloc);
    CK_ASSERT_TRUE(inserted);
  }

  return new_alloc;
}

void TestMainAllocator::Free(Void* ptr) {
  MappedSlab* slab = test_fixture_->SlabMap().FindSlab(PageId::FromPtr(ptr));
  CK_ASSERT_NE(slab, nullptr);
  if (slab->Type() == SlabType::kMmap) {
    size_t res = test_fixture_->mmap_blocks_.erase(ptr);
    CK_ASSERT_EQ(res, 1);
  }

  auto it = test_fixture_->allocations_.find(ptr);
  CK_ASSERT_FALSE(it == test_fixture_->allocations_.end());

  test_fixture_->allocations_.erase(it);
  return main_allocator_.Free(ptr);
}

size_t TestMainAllocator::AllocSize(Void* ptr) {
  return main_allocator_.AllocSize(ptr);
}

void TestMainAllocator::HandleAllocation(Void* alloc, size_t user_size) {
  uint64_t magic = folly::ThreadLocalPRNG()();
  MainAllocatorFixture::FillMagic(alloc, user_size, magic);

  auto [it, inserted] =
      test_fixture_->allocations_.insert({ alloc, { user_size, magic } });
  CK_ASSERT_TRUE(inserted);

  MappedSlab* slab = test_fixture_->SlabMap().FindSlab(PageId::FromPtr(alloc));
  CK_ASSERT_NE(slab, nullptr);
  if (slab->Type() == SlabType::kMmap) {
    auto [_, inserted] = test_fixture_->mmap_blocks_.insert(alloc);
    CK_ASSERT_TRUE(inserted);
  }
}

absl::Status MainAllocatorFixture::ValidateHeap() {
  // Validate that allocations do not overlap and that their magic numbers are
  // intact.
  for (auto it = allocations_.cbegin(); it != allocations_.cend();) {
    auto [alloc, meta] = *it;
    auto [size, magic] = meta;

    // Check for overlapping allocations
    auto next_it = it;
    ++next_it;
    if (next_it != allocations_.cend()) {
      auto [alloc2, meta2] = *next_it;
      auto [size2, magic2] = meta2;

      if (alloc2 < PtrAdd(alloc, size)) {
        return FailedTest(
            "Allocation %p of size %zu overlaps with allocation %p of size %zu",
            alloc, size, alloc2, size2);
      }
    }

    // Check magic bytes of block.
    RETURN_IF_ERROR(CheckMagic(alloc, size, magic));

    MappedSlab* slab = SlabMap().FindSlab(PageId::FromPtr(alloc));
    size_t derived_size = MainAllocator().AllocSize(alloc);
    size_t aligned_size;
    switch (slab->Type()) {
      case SlabType::kSingleAlloc:
      case SlabType::kMmap: {
        aligned_size = AlignUp(size, kPageSize);
        break;
      }
      case SlabType::kBlocked: {
        aligned_size =
            Block::UserSizeForBlockSize(Block::BlockSizeForUserSize(size));
        break;
      }
      case SlabType::kSmall: {
        aligned_size = SizeClass::FromUserDataSize(size).SliceSize();
        break;
      }
      case SlabType::kFree:
      case SlabType::kUnmapped: {
        return FailedTest("Unexpected non-allocated slab %v", *slab);
      }
    }
    if (aligned_size != derived_size) {
      if (slab->Type() != SlabType::kBlocked || aligned_size > derived_size ||
          aligned_size + Block::kMinBlockSize <= derived_size) {
        return FailedTest(
            "Allocated block at %p of size %zu in %v has the wrong size when "
            "looked up with MainAllocator::AllocSize: found %zu, expected %zu "
            "- %zu",
            alloc, size, *slab, derived_size, aligned_size,
            slab->Type() == SlabType::kBlocked
                ? aligned_size + Block::kMinBlockSize
                : aligned_size);
      }
    }

    // Mmapped slabs are not validated by the slab manager. Check that their
    // slab map entry is correct.
    if (slab->Type() == SlabType::kMmap) {
      MappedSlab* mapped_slab = SlabMap().FindSlab(slab->StartId());
      if (mapped_slab != slab) {
        return FailedTest(
            "Page %v of %v does not map to the correct slab metadata: %v",
            slab->StartId(), *slab, mapped_slab);
      }

      SizeClass size_class = SlabMap().FindSizeClass(slab->StartId());
      if (size_class != SizeClass::Nil()) {
        return FailedTest(
            "Found non-nil size class for mmap slab %v at %v, size class %v",
            *slab, slab->StartId(), size_class);
      }
    }

    it = next_it;
  }

  // Iterate over the heap and validate that all encountered slabs are
  // considered allocated by the metadata allocator, and that no slabs are
  // missing.
  uint64_t num_slabs = 0;
  for (MappedSlab* mapped_slab : slab_manager_test_fixture_->SlabsInHeap()) {
    if (!metadata_manager_test_fixture_->AllocatedSlabMeta().contains(
            mapped_slab)) {
      return FailedTest(
          "Encountered non-allocated slab metadata %v in slab map!",
          mapped_slab);
    }
    num_slabs++;
  }

  for (const Void* mmap_alloc : mmap_blocks_) {
    MappedSlab* mapped_slab = SlabMap().FindSlab(PageId::FromPtr(mmap_alloc));
    if (!metadata_manager_test_fixture_->AllocatedSlabMeta().contains(
            mapped_slab)) {
      return FailedTest(
          "Encountered non-allocated slab metadata %v in slab map for mmapped "
          "block %p!",
          mapped_slab, mmap_alloc);
    }
    num_slabs++;
  }

  if (num_slabs != metadata_manager_test_fixture_->AllocatedSlabMeta().size()) {
    return FailedTest(
        "Encountered %v slabs while iterating over the heap, but expected %v, "
        "did we leak a slab metadata?",
        num_slabs, metadata_manager_test_fixture_->AllocatedSlabMeta().size());
  }

  return absl::OkStatus();
}

absl::Status MainAllocatorFixture::ValidateEmpty() {
  if (!allocations_.empty()) {
    return FailedTest("Expected empty allocations set");
  }

  return absl::OkStatus();
}

/* static */
void MainAllocatorFixture::FillMagic(void* allocation, size_t size,
                                     uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(allocation);
  uint8_t* end = start + size;

  for (uint8_t* ptr = start; ptr != end; ptr++) {
    uint32_t byte = static_cast<uint32_t>((ptr - start) % 8);
    *ptr = (magic >> (8 * byte)) & 0xff;
  }
}

absl::Status MainAllocatorFixture::CheckMagic(void* allocation, size_t size,
                                              uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(allocation);
  uint8_t* end = start + size;

  for (uint8_t* ptr = start; ptr != end; ptr++) {
    uint32_t byte = static_cast<uint32_t>((ptr - start) % 8);
    if (*ptr != ((magic >> (8 * byte)) & 0xff)) {
      return FailedTest("Allocation %p was dirtied starting from offset %lu",
                        allocation, ptr - start);
    }
  }

  return absl::OkStatus();
}

}  // namespace ckmalloc
