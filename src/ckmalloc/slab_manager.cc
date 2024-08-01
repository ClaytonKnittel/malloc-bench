#include "src/ckmalloc/slab_manager.h"

#include <unistd.h>

namespace ckmalloc {

const size_t SlabManager::kSlabSize = []() {
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
}();

}
