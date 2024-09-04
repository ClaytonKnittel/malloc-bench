#include "src/heap_factory.h"

#include <memory>

#include "util/absl_util.h"

#include "src/heap_interface.h"

namespace bench {

absl::StatusOr<std::pair<size_t, Heap*>> HeapFactory::NewInstance(size_t size) {
  DEFINE_OR_RETURN(std::unique_ptr<Heap>, heap, MakeHeap(size));
  size_t idx = heaps_.size();
  heaps_.emplace_back(std::move(heap));
  return std::make_pair(idx, Instance(idx));
}

Heap* HeapFactory::Instance(size_t idx) {
  if (idx >= heaps_.size()) {
    return nullptr;
  }
  return heaps_[idx].get();
}

const Heap* HeapFactory::Instance(size_t idx) const {
  if (idx >= heaps_.size()) {
    return nullptr;
  }
  return heaps_[idx].get();
}

const std::vector<std::unique_ptr<Heap>>& HeapFactory::Instances() const {
  return heaps_;
}

void HeapFactory::Reset() {
  heaps_.clear();
}

}  // namespace bench
