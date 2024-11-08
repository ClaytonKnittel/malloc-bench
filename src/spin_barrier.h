#pragma once

#include <atomic>
#include <cstdint>

namespace bench {

class SpinBarrier {
 public:
  explicit SpinBarrier(uint32_t cnt);

  void arrive_and_wait();

  void arrive_and_drop();

 private:
  std::atomic<uint64_t> lower_bound_ = 0;
  std::atomic<uint64_t> upper_bound_;
  std::atomic<uint64_t> count_ = 0;
};

}  // namespace bench