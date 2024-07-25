#pragma once

#include <cstddef>
#include <cstdint>

#include "absl/status/statusor.h"

namespace bench {

class FakeHeap {
  friend class CorrectnessChecker;

 public:
  FakeHeap(FakeHeap&& heap) noexcept;
  ~FakeHeap();

  FakeHeap& operator=(FakeHeap&& heap) noexcept;

  void swap(FakeHeap& other) noexcept;

  static FakeHeap* GlobalInstance();

  // Resets the heap and returns a pointer to the beginning of the heap.
  void* Reset();

  // Increments the size of the heap by `increment` bytes. The heap starts off
  // empty and must be increased by calling `sbrk()` before anything can be
  // written to it.
  //
  // On success, `sbrk()` returns the previous program break. (If the break was
  // increased, then this value is a pointer to the start of the newly allocated
  // memory). On error, `nullptr` is returned, and errno is set to ENOMEM.
  void* sbrk(intptr_t increment);

 private:
  explicit FakeHeap(void* heap_start);

  static absl::StatusOr<FakeHeap> Initialize();

  // Returns the start of the heap.
  void* Start() const {
    return heap_start_;
  }

  // Returns the end of the heap.
  void* End() const {
    return heap_end_;
  }

  // Max heap size is 100 MB.
  static constexpr size_t kHeapSize = 100 * (1 << 20);

  // The global heap instance.
  static FakeHeap global_heap_;

  void* heap_start_;
  void* heap_end_;
};

}  // namespace bench
