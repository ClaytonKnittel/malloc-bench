#pragma once

#include <cstring>

namespace jsmalloc {
namespace math {

/** Rounds size up to the nearest 16 byte boundary. */
size_t round_16b(size_t size);

}  // namespace math
}  // namespace jsmalloc
