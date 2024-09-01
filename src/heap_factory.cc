#include "src/heap_factory.h"

#include "util/absl_util.h"

#include "src/heap_interface.h"
#include "src/mmap_heap.h"

namespace bench {

absl::StatusOr<std::pair<size_t, Heap*>> HeapFactory::NewInstance(size_t size) {
  DEFINE_OR_RETURN(MMapHeap, heap, MMapHeap::NewInstance(size));
  size_t idx = heaps_.size();
  heaps_.emplace_back(std::move(heap));
  return std::make_pair(idx, Instance(idx));
}

Heap* HeapFactory::Instance(size_t idx) {
  if (idx >= heaps_.size()) {
    return nullptr;
  }
  return &heaps_[idx];
}

const std::vector<MMapHeap>& HeapFactory::Instances() const {
  return heaps_;
}

void HeapFactory::Reset() {
  heaps_.clear();
}

}  // namespace bench
