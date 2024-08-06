#include "src/ckmalloc/slab.h"

#include "src/ckmalloc/util.h"

namespace ckmalloc {

uint32_t Slab::Pages() const {
  CK_ASSERT(type_ != SlabType::kFree);
  switch (type_) {
    case SlabType::kLarge: {
      return allocated.large.n_pages_;
    }
    case SlabType::kSmall: {
      return 1;
    }
    case SlabType::kFree: {
      CK_UNREACHABLE();
    }
  }
}

}  // namespace ckmalloc
