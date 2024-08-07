#include "src/jsmalloc/util/math.h"

namespace jsmalloc {
namespace math {

size_t round_16b(size_t size) {
  return (size + 0xf) & ~0xf;
}

}  // namespace math
}  // namespace jsmalloc
