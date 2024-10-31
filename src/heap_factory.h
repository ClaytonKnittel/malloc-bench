#pragma once

#include <cstddef>
#include <memory>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include "src/heap_interface.h"

namespace bench {

// TODO: make this thread-safe
class HeapFactory {
 public:
  HeapFactory() = default;
  HeapFactory(HeapFactory&&) = default;
  virtual ~HeapFactory() = default;

  // Allocates a new heap of the requested size, returning the index of the new
  // heap and a pointer to it.
  absl::StatusOr<Heap*> NewInstance(size_t size);

  // Deletes a heap at the given index.
  absl::Status DeleteInstance(Heap* heap);

  const absl::flat_hash_set<std::unique_ptr<Heap>>& Instances() const;

  // Clears the heap factory and deletes all allocated heaps.
  void Reset();

 protected:
  virtual absl::StatusOr<std::unique_ptr<Heap>> MakeHeap(size_t size) = 0;

 private:
  absl::flat_hash_set<std::unique_ptr<Heap>> heaps_;
};

}  // namespace bench
