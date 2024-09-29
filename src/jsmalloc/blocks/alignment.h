#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {

class Alignment {
 public:
  static Alignment Of(size_t alignment) {
    alignment = alignment == 0 ? 1 : alignment;

    DCHECK_EQ(std::popcount(alignment), 1);
    Alignment a(std::countr_zero(alignment));

    DCHECK_EQ(a.Get(), alignment);
    return a;
  }

  size_t Get() const {
    return 1 << pow_;
  }

 private:
  explicit Alignment(uint8_t pow) : pow_(pow) {}

  uint8_t pow_;
};

}  // namespace jsmalloc
