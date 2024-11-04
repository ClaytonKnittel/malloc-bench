#include "src/heap_interface.h"

#include <atomic>
#include <cerrno>

namespace bench {

void* Heap::sbrk(intptr_t increment) {
  if (increment < 0) {
    errno = EINVAL;
    return nullptr;
  }

  void* old_heap_end = heap_end_.load(std::memory_order_relaxed);
  do {
    if ((static_cast<uint8_t*>(old_heap_end) -
         static_cast<uint8_t*>(heap_start_)) +
            increment >
        max_size_) {
      errno = ENOMEM;
      return nullptr;
    }
  } while (!heap_end_.compare_exchange_weak(
      old_heap_end, static_cast<uint8_t*>(old_heap_end) + increment,
      std::memory_order_relaxed, std::memory_order_relaxed));
  return old_heap_end;
}

}  // namespace bench
