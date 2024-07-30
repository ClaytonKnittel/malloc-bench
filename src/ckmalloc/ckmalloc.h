#pragma once

#include <cstddef>

namespace ckmalloc {

void* malloc(size_t size);

void* calloc(size_t nmemb, size_t size);

void* realloc(void* ptr, size_t size);

void free(void* ptr);

}  // namespace ckmalloc
