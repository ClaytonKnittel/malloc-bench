#include "src/heap_factory.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "util/absl_util.h"

#include "src/heap_interface.h"

namespace bench {

absl::StatusOr<Heap*> HeapFactory::NewInstance(size_t size) {
  DEFINE_OR_RETURN(std::unique_ptr<Heap>, heap, MakeHeap(size));
  Heap* heap_ptr = heap.get();
  heaps_.emplace(std::move(heap));
  return heap_ptr;
}

absl::Status HeapFactory::DeleteInstance(Heap* heap) {
  auto it = heaps_.find(heap);
  if (it == heaps_.end()) {
    return absl::NotFoundError(absl::StrFormat("Heap not found: %p", heap));
  }

  heaps_.erase(it);
  return absl::OkStatus();
}

const absl::flat_hash_set<std::unique_ptr<Heap>>& HeapFactory::Instances()
    const {
  return heaps_;
}

void HeapFactory::Reset() {
  heaps_.clear();
}

}  // namespace bench
