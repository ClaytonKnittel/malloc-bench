#include "src/heap_factory.h"

#include <memory>

#include "util/absl_util.h"

#include "src/heap_interface.h"
#include "src/mmap_heap.h"

namespace bench {

absl::StatusOr<std::pair<size_t, Heap*>> HeapFactory::NewInstance(size_t size) {
  DEFINE_OR_RETURN(MMapHeap, heap, MMapHeap::New(size));
  size_t idx = heaps_.size();
  heaps_.emplace_back(std::make_unique<MMapHeap>(std::move(heap)));
  return std::make_pair(idx, Instance(idx));
}

Heap* HeapFactory::Instance(size_t idx) {
  if (idx >= heaps_.size()) {
    return nullptr;
  }
  return heaps_[idx].get();
}

const std::vector<std::unique_ptr<MMapHeap>>& HeapFactory::Instances() const {
  return heaps_;
}

void HeapFactory::Reset() {
  heaps_.clear();
}

}  // namespace bench
