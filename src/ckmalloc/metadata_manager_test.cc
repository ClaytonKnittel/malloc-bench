#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "util/absl_util.h"
#include "util/gtest_util.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/testlib.h"
#include "src/rng.h"

namespace ckmalloc {

using util::IsOk;
using util::IsOkAndHolds;

class MetadataManagerTest : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  MetadataManagerTest()
      : heap_(kNumPages),
        slab_manager_(&heap_, &slab_map_),
        metadata_manager_(&slab_map_, &slab_manager_),
        rng_(2021, 5) {}

  TestHeap& Heap() {
    return heap_;
  }

  TestSlabManager& SlabManager() {
    return slab_manager_;
  }

  TestMetadataManager& MetadataManager() {
    return metadata_manager_;
  }

  absl::StatusOr<size_t> SlabMetaFreelistLength() const;

  absl::StatusOr<void*> Alloc(size_t size, size_t alignment = 1);

  static void FillMagic(void* block, size_t size, uint64_t magic);
  static absl::Status CheckMagic(void* block, size_t size, uint64_t magic);

  absl::Status ValidateHeap();

 private:
  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;
  TestMetadataManager metadata_manager_;

  util::Rng rng_;

  // Maps allocations to their sizes and the magic numbers that they are filled
  // with.
  absl::flat_hash_map<void*, std::pair<size_t, uint64_t>> allocated_blocks_;
};

absl::StatusOr<size_t> MetadataManagerTest::SlabMetaFreelistLength() const {
  constexpr size_t kMaxReasonableLength = 10000;
  size_t length = 0;
  for (Slab* free_slab = metadata_manager_.last_free_slab_;
       free_slab != nullptr && length < kMaxReasonableLength;
       free_slab = free_slab->NextUnmappedSlab(), length++)
    ;

  return length == kMaxReasonableLength
             ? absl::FailedPreconditionError(
                   "Slab metadata freelist appears to have a cycle")
             : absl::StatusOr<size_t>(length);
}

absl::StatusOr<void*> MetadataManagerTest::Alloc(size_t size,
                                                 size_t alignment) {
  void* result = metadata_manager_.Alloc(size, alignment);
  if (result == nullptr) {
    return nullptr;
  }

  // Check that the pointer is aligned relative to the heap start. The heap will
  // be page-aligned in production, but may not be in tests.
  if (((reinterpret_cast<intptr_t>(result) -
        reinterpret_cast<intptr_t>(heap_.Start())) &
       (alignment - 1)) != 0) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Pointer returned from Alloc not aligned properly: "
                        "pointer %p, size %zu, alignment %zu",
                        result, size, alignment));
  }

  if (result < heap_.Start() ||
      static_cast<uint8_t*>(result) + size > heap_.End()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Block allocated outside range of heap: returned %p of "
                        "size %zu, heap ranges from %p to %p",
                        result, size, heap_.Start(), heap_.End()));
  }

  for (const auto& [ptr, meta] : allocated_blocks_) {
    if (ptr < static_cast<uint8_t*>(result) + size &&
        result < static_cast<uint8_t*>(ptr) + meta.first) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated block overlaps with already allocated block: returned %p "
          "of size %zu, overlaps with %p of size %zu",
          result, size, ptr, meta.first));
    }
  }

  uint64_t magic = rng_.GenRand64();
  FillMagic(result, size, magic);

  allocated_blocks_.insert({ result, { size, magic } });

  return result;
}

/* static */
void MetadataManagerTest::FillMagic(void* block, size_t size, uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(block);

  for (size_t i = 0; i < size; i++) {
    uint8_t magic_byte = (magic >> ((i % 8) * 8)) & 0xff;
    start[i] = magic_byte;
  }
}

/* static */
absl::Status MetadataManagerTest::CheckMagic(void* block, size_t size,
                                             uint64_t magic) {
  uint8_t* start = reinterpret_cast<uint8_t*>(block);

  for (size_t i = 0; i < size; i++) {
    uint8_t magic_byte = (magic >> ((i % 8) * 8)) & 0xff;
    start[i] = magic_byte;
    if (start[i] != magic_byte) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Allocated block %p of size %zu was dirtied starting from offset %zu",
          start, size, i));
    }
  }

  return absl::OkStatus();
}

absl::Status MetadataManagerTest::ValidateHeap() {
  if (Heap().Size() % kPageSize != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Expected heap size to be a multiple of page size, but was %zu",
        Heap().Size()));
  }

  for (const auto& [block, meta] : allocated_blocks_) {
    const auto& [size, magic] = meta;
    RETURN_IF_ERROR(CheckMagic(block, size, magic));
  }

  return absl::OkStatus();
}

TEST_F(MetadataManagerTest, TestEmpty) {
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_THAT(SlabMetaFreelistLength(), IsOkAndHolds(0));
}

TEST_F(MetadataManagerTest, TestAllocOnce) {
  ASSERT_OK_AND_DEFINE(void*, value, Alloc(16));
  EXPECT_EQ(value, Heap().Start());
}

TEST_F(MetadataManagerTest, TestAllocAdjacent) {
  ASSERT_OK_AND_DEFINE(void*, v1, Alloc(7));
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(41));
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(60));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v2) - reinterpret_cast<uint8_t*>(v1), 7);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v3) - reinterpret_cast<uint8_t*>(v2),
            41);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, TestAllocAligned) {
  ASSERT_OK_AND_DEFINE(void*, v1, Alloc(7));
  // Should range from 8 - 55 (inclusive)
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(48, 8));
  // Should range from 64 - 127 (inclusive)
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(64, 64));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v2) - reinterpret_cast<uint8_t*>(v1), 8);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(v3) - reinterpret_cast<uint8_t*>(v2),
            56);
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, TestLarge) {
  ASSERT_OK_AND_DEFINE(void*, value, Alloc(kPageSize));
  EXPECT_EQ(value, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, TestExtraLarge) {
  ASSERT_OK_AND_DEFINE(void*, value, Alloc(11 * kPageSize));
  EXPECT_EQ(value, Heap().Start());
  EXPECT_THAT(ValidateHeap(), IsOk());
}

TEST_F(MetadataManagerTest, AllocateAndStay) {
  ASSERT_THAT(Alloc(kPageSize / 2).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(3 * kPageSize / 4));
  // v2 should be allocated in a new page by itself.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the first slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize / 2));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + kPageSize / 2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateAndSwitch) {
  ASSERT_THAT(Alloc(3 * kPageSize / 4).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize / 2));
  // v2 should be allocated in a new page by itself.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the second slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize / 2));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 3 * kPageSize / 2);
  EXPECT_EQ(Heap().Size(), 2 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateLargeAndStay) {
  ASSERT_THAT(Alloc(32).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize + 64));
  // v2 should be allocated in a new slab by itself since it is so large.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the first slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize - 32));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 32);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateLargeAndSwitch) {
  ASSERT_THAT(Alloc(64).status(), IsOk());
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize + 32));
  // v2 should be allocated in a new slab by itself since it is so large.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(1)));
  EXPECT_THAT(ValidateHeap(), IsOk());

  // Since the remainder in the second slab was higher, it should continue to be
  // allocated from.
  ASSERT_OK_AND_DEFINE(void*, v3, Alloc(kPageSize - 32));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(v3, static_cast<uint8_t*>(Heap().Start()) + 2 * kPageSize + 32);
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

TEST_F(MetadataManagerTest, AllocateWithOtherAllocators) {
  ASSERT_THAT(Alloc(kPageSize).status(), IsOk());

  // Allocate a phony slab right after the one just allocated.
  auto res = SlabManager().Alloc(1, SlabType::kSmall);
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res.value().first, PageId(1));

  // Allocate another slab-sized metadata alloc.
  ASSERT_OK_AND_DEFINE(void*, v2, Alloc(kPageSize));
  // v2 should be allocated in a new slab after the two already-allocated slabs.
  EXPECT_EQ(v2, SlabManager().PageStartFromId(PageId(2)));
  EXPECT_THAT(ValidateHeap(), IsOk());
  EXPECT_EQ(Heap().Size(), 3 * kPageSize);
}

}  // namespace ckmalloc
