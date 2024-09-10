#pragma once

#include <cstddef>

namespace ckmalloc {

class LocalCache {
 public:
  LocalCache() = default;

  void* FindAlloc(size_t user_size);

 private:
};

}  // namespace ckmalloc
