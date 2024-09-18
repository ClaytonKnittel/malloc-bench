#include "src/ckmalloc/size_class.h"

#include <ostream>

#include "src/ckmalloc/common.h"

namespace ckmalloc {

std::ostream& operator<<(std::ostream& ostr, ckmalloc::SizeClass size_class) {
  return ostr << "[" << size_class.SliceSize() << "]";
}

}  // namespace ckmalloc
