#pragma once

#include <cstdint>

namespace ckmalloc {

class Block {
 public:
 private:
  uint64_t header_;
};

}  // namespace ckmalloc
