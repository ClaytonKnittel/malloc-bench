#include "src/spin_barrier.h"

#include <atomic>
#include <cassert>

namespace bench {

SpinBarrier::SpinBarrier(uint32_t cnt) : upper_bound_(cnt) {}

void SpinBarrier::arrive_and_wait() {
  Count count = count_.fetch_add(1, std::memory_order_relaxed);
  const Count barrier_complete(total_cnt_, count.generation);
  const Count new_count(0, count.generation + 1);
  uint64_t expected = barrier_complete.Encoding();
  while (!count_.compare_exchange_weak(expected, new_count.Encoding(),
                                       std::memory_order_relaxed,
                                       std::memory_order_relaxed)) {
    total_cnt = total_cnt_.load(std::memory_order_relaxed);
  }
}

void SpinBarrier::arrive_and_drop() {}

}  // namespace bench