#pragma once

#include <string>

#include "absl/container/flat_hash_map.h"

#include "src/ckmalloc/common.h"
#include "src/ckmalloc/metadata_manager.h"
#include "src/ckmalloc/page_id.h"
#include "src/ckmalloc/slab.h"
#include "src/ckmalloc/slab_manager.h"
#include "src/ckmalloc/slab_map.h"
#include "src/heap_interface.h"

namespace ckmalloc {

class HeapPrinter {
 public:
  HeapPrinter(const bench::Heap* heap, const SlabMap* slab_map,
              const SlabManager* slab_manager,
              const MetadataManager* metadata_manager);

  HeapPrinter& WithHighlightAddr(void* addr, const char* color_fmt);

  std::string Print();

 private:
  static constexpr size_t kMaxRowLength = kPageSize / kDefaultAlignment / 2;

  static std::string PrintMetadata(PageId page_id);

  static std::string PrintFree(const FreeSlab* slab);

  std::string PrintSmall(const SmallSlab* slab);

  std::string PrintBlocked(const BlockedSlab* slab);

  std::string PrintSingleAlloc(const SingleAllocSlab* slab);

  const bench::Heap* const heap_;
  const SlabMap* const slab_map_;
  const SlabManager* const slab_manager_;
  const MetadataManager* const metadata_manager_;

  // Addresses to highlight using the given format strings.
  absl::flat_hash_map<void*, const char*> highlight_addrs_;
};

}  // namespace ckmalloc
