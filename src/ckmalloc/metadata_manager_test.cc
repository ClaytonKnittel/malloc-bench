#include "util/gtest_util.h"

#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/testlib.h"

namespace ckmalloc {

using util::IsOk;

class MetadataManagerTest : public CkMallocTest {
 public:
  static constexpr size_t kNumPages = 64;

  MetadataManagerTest()
      : heap_(kNumPages),
        slab_manager_(&heap_, &slab_map_),
        metadata_manager_(&slab_map_, &slab_manager_) {}

  TestHeap& Heap() {
    return heap_;
  }

  TestMetadataManager& MetadataManager() {
    return metadata_manager_;
  }

  absl::Status ValidateHeap();

 private:
  TestHeap heap_;
  TestSlabMap slab_map_;
  TestSlabManager slab_manager_;
  TestMetadataManager metadata_manager_;
};

absl::Status MetadataManagerTest::ValidateHeap() {
  if (Heap().Size() % kPageSize != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Expected heap size to be a multiple of page size, but was %zu",
        Heap().Size()));
  }

  return absl::OkStatus();
}

TEST_F(MetadataManagerTest, TestEmpty) {
  EXPECT_THAT(ValidateHeap(), IsOk());
}

}  // namespace ckmalloc
