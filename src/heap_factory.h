#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"

#include "src/heap_interface.h"
#include "src/util.h"

namespace bench {

class HeapFactory {
 public:
  HeapFactory() = default;
  HeapFactory(const HeapFactory&) = delete;
  virtual ~HeapFactory() = default;

  // Allocates a new heap of the requested size, returning the index of the new
  // heap and a pointer to it.
  absl::StatusOr<Heap*> NewInstance(size_t size);

  // Deletes a heap at the given index.
  absl::Status DeleteInstance(Heap* heap);

  template <typename ReturnVal, typename Fn>
  requires std::is_invocable_r_v<
      ReturnVal, Fn, const absl::flat_hash_set<std::unique_ptr<Heap>>&>
  ReturnVal WithInstances(const Fn& fn);

  // Clears the heap factory and deletes all allocated heaps.
  void Reset();

 protected:
  virtual absl::StatusOr<std::unique_ptr<Heap>> MakeHeap(size_t size) = 0;

 private:
  absl::Mutex mutex_;
  absl::flat_hash_set<std::unique_ptr<Heap>> heaps_ BENCH_GUARDED_BY(mutex_);
};

template <typename ReturnVal, typename Fn>
requires std::is_invocable_r_v<
    ReturnVal, Fn, const absl::flat_hash_set<std::unique_ptr<Heap>>&>
ReturnVal HeapFactory::WithInstances(const Fn& fn) {
  absl::ReaderMutexLock lock(&mutex_);
  return fn(heaps_);
}

}  // namespace bench
