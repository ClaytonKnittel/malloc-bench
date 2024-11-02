#include "src/heap_factory.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "util/absl_util.h"

#include "src/heap_interface.h"

namespace bench {

absl::StatusOr<Heap*> HeapFactory::NewInstance(size_t size) {
  DEFINE_OR_RETURN(std::unique_ptr<Heap>, heap, MakeHeap(size));
  Heap* heap_ptr = heap.get();

  absl::WriterMutexLock lock(&mutex_);
  heaps_.emplace(std::move(heap));
  return heap_ptr;
}

absl::Status HeapFactory::DeleteInstance(Heap* heap) {
  absl::WriterMutexLock lock(&mutex_);
  auto it = heaps_.find(heap);
  if (it == heaps_.end()) {
    return absl::NotFoundError(absl::StrFormat("Heap not found: %p", heap));
  }

  heaps_.erase(it);
  return absl::OkStatus();
}

void HeapFactory::Reset() {
  absl::WriterMutexLock lock(&mutex_);
  heaps_.clear();
}

}  // namespace bench
