#include "src/heap_interface.h"

#include <cerrno>

namespace bench {

void* Heap::sbrk(intptr_t increment) {
  void* old_heap_end = heap_end_;

  if (increment < 0) {
    errno = EINVAL;
    return nullptr;
  }
  if ((static_cast<uint8_t*>(old_heap_end) -
       static_cast<uint8_t*>(heap_start_)) +
          increment >
      max_size_) {
    errno = ENOMEM;
    return nullptr;
  }

  heap_end_ = static_cast<uint8_t*>(old_heap_end) + increment;
  return old_heap_end;
}

}  // namespace bench
