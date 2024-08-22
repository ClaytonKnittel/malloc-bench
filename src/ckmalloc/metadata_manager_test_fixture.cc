#include "src/ckmalloc/metadata_manager_test_fixture.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"

#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/ckmalloc/util.h"
#include "src/rng.h"

namespace ckmalloc {

Slab* TestMetadataAlloc::SlabAlloc() {
  return manager_->NewSlabMeta();
}

void TestMetadataAlloc::SlabFree(MappedSlab* slab) {
  manager_->FreeSlabMeta(slab);
}

void* TestMetadataAlloc::Alloc(size_t size, size_t alignment) {
  return manager_->Alloc(size, alignment);
};

TestMetadataManager::TestMetadataManager(MetadataManagerFixture* test_fixture,
                                         TestSlabMap* slab_map,
                                         TestSlabManager* slab_manager)
    : test_fixture_(test_fixture),
      metadata_manager_(slab_map, slab_manager, PageId::Zero()) {}

void* TestMetadataManager::Alloc(size_t size, size_t alignment) {
  void* block = metadata_manager_.Alloc(size, alignment);
  if (block == nullptr) {
    return nullptr;
  }

  auto [it, inserted] =
      test_fixture_->allocated_blocks_.insert({ block, size });
  CK_ASSERT_TRUE(inserted);

  return block;
}

Slab* TestMetadataManager::NewSlabMeta() {
  Slab* slab = metadata_manager_.NewSlabMeta();

  // If the slab metadata freelist is not empty, then something from it must be
  // allocated.
  if (!test_fixture_->freed_slab_metadata_.empty()) {
    auto it = test_fixture_->freed_slab_metadata_.find(slab);
    CK_ASSERT_TRUE(it != test_fixture_->freed_slab_metadata_.end());
    test_fixture_->freed_slab_metadata_.erase(it);
  }

  return slab;
}

void TestMetadataManager::FreeSlabMeta(MappedSlab* slab) {
  metadata_manager_.FreeSlabMeta(slab);

  auto [_, inserted] = test_fixture_->freed_slab_metadata_.insert(
      static_cast<Slab*>(slab)->ToUnmapped());
  CK_ASSERT_TRUE(inserted);
}

absl::StatusOr<size_t> MetadataManagerFixture::SlabMetaFreelistLength() const {
  constexpr size_t kMaxReasonableLength = 10000;
  size_t length = 0;
  for (UnmappedSlab* free_slab = MetadataManager().Underlying().last_free_slab_;
       free_slab != nullptr && length < kMaxReasonableLength;
       free_slab = free_slab->NextUnmappedSlab(), length++)
    ;

  return length == kMaxReasonableLength
             ? FailedTest("Slab metadata freelist appears to have a cycle")
             : absl::StatusOr<size_t>(length);
}

absl::StatusOr<void*> MetadataManagerFixture::Alloc(size_t size,
                                                    size_t alignment) {
  void* result = MetadataManager().Alloc(size, alignment);
  if (result == nullptr) {
    return nullptr;
  }

  RETURN_IF_ERROR(TraceBlockAllocation(result, size, alignment));
  return result;
}

absl::StatusOr<Slab*> MetadataManagerFixture::NewSlabMeta() {
  Slab* slab = MetadataManager().NewSlabMeta();
  if (slab == nullptr) {
    return nullptr;
  }

  RETURN_IF_ERROR(TraceBlockAllocation(slab, sizeof(Slab), alignof(Slab)));
  return slab;
}

absl::Status MetadataManagerFixture::FreeSlabMeta(Slab* slab) {
  auto alloc_it = allocated_blocks_.find(slab);
  CK_ASSERT_FALSE(alloc_it == allocated_blocks_.end());
  if (alloc_it->second != sizeof(Slab)) {
    return FailedTest(
        "Slab block in allocated blocks map not the correct size: %v, expected "
        "block size %zu, found size %zu",
        *slab, sizeof(Slab), alloc_it->second);
  }

  auto magic_it = block_magics_.find(slab);
  CK_ASSERT_FALSE(magic_it == block_magics_.end());
  RETURN_IF_ERROR(CheckMagic(slab, sizeof(Slab), magic_it->second));
  block_magics_.erase(magic_it);

  MetadataManager().FreeSlabMeta(slab->ToMapped());
  return absl::OkStatus();
}

/* static */
void MetadataManagerFixture::FillMagic(void* block, size_t size,
                                       uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(block);

  for (size_t i = 0; i < size; i++) {
    uint8_t magic_byte = (magic >> ((i % 8) * 8)) & 0xff;
    start[i] = magic_byte;
  }
}

absl::Status MetadataManagerFixture::CheckMagic(void* block, size_t size,
                                                uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(block);

  for (size_t i = 0; i < size; i++) {
    uint8_t magic_byte = (magic >> ((i % 8) * 8)) & 0xff;
    start[i] = magic_byte;
    if (start[i] != magic_byte) {
      return FailedTest(
          "Allocated block %p of size %zu was dirtied starting from offset %zu",
          start, size, i);
    }
  }

  return absl::OkStatus();
}

absl::Status MetadataManagerFixture::ValidateHeap() {
  for (const auto& [block, magic] : block_magics_) {
    auto it = allocated_blocks_.find(block);
    CK_ASSERT_FALSE(it == allocated_blocks_.end());
    RETURN_IF_ERROR(CheckMagic(block, it->second, magic));
  }

  constexpr size_t kMaxReasonableFreedSlabMetas = 10000;
  size_t n_free_slab_meta = 0;
  for (const UnmappedSlab* slab =
           MetadataManager().Underlying().last_free_slab_;
       slab != nullptr && n_free_slab_meta < kMaxReasonableFreedSlabMetas;
       slab = slab->NextUnmappedSlab()) {
    if (!freed_slab_metadata_.contains(slab)) {
      return FailedTest(
          "Encountered freed slab metadata in freelist which should not be: %v",
          *slab);
    }

    if (slab->Type() != SlabType::kUnmapped) {
      return FailedTest(
          "Expected slab metadata in freelist to be unmapped, found %v", *slab);
    }

    n_free_slab_meta++;
  }

  if (n_free_slab_meta == kMaxReasonableFreedSlabMetas) {
    return FailedTest(
        "Detected cycle in slab metadata freelist after searching %zu elements",
        n_free_slab_meta);
  }

  absl::flat_hash_set<MappedSlab*> visited_slabs;
  PageId page = PageId::Zero();
  PageId end = slab_manager_test_fixture_->HeapEndId();
  while (page < end) {
    MappedSlab* slab = SlabMap().FindSlab(page);
    if (slab == nullptr) {
      // This must be a metadata slab.
      page += 1;
      continue;
    }

    if (freed_slab_metadata_.contains(slab)) {
      return FailedTest("Found allocated slab %v in the freelist.", *slab);
    }

    page += slab->Pages();
  }

  return absl::OkStatus();
}

absl::Status MetadataManagerFixture::TraceBlockAllocation(void* block,
                                                          size_t size,
                                                          size_t alignment) {
  // Check that the pointer is aligned relative to the heap start. The heap
  // will be page-aligned in production, but may not be in tests.
  if ((PtrDistance(block, Heap().Start()) & (alignment - 1)) != 0) {
    return FailedTest(
        "Pointer returned from Alloc not aligned properly: pointer %p, size "
        "%zu, alignment %zu",
        block, size, alignment);
  }

  if (block < Heap().Start() ||
      static_cast<uint8_t*>(block) + size > Heap().End()) {
    return FailedTest(
        "Block allocated outside range of heap: returned %p of size %zu, heap "
        "ranges from %p to %p",
        block, size, Heap().Start(), Heap().End());
  }

  for (const auto& [ptr, ptr_size] : allocated_blocks_) {
    // Don't check for collision with ourselves.
    if (ptr == block) {
      continue;
    }

    if (ptr < static_cast<uint8_t*>(block) + size &&
        block < static_cast<uint8_t*>(ptr) + ptr_size) {
      return FailedTest(
          "Allocated block overlaps with already allocated block: returned %p "
          "of size %zu, overlaps with %p of size %zu",
          block, size, ptr, ptr_size);
    }
  }

  uint64_t magic = rng_.GenRand64();
  FillMagic(block, size, magic);
  block_magics_.insert({ block, magic });

  return absl::OkStatus();
}

}  // namespace ckmalloc
