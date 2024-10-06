#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "src/heap_factory.h"

namespace bench {

void initialize_heap(HeapFactory& heap_factory);

void* malloc(size_t size, size_t alignment = 0);

void* calloc(size_t nmemb, size_t size);

void* realloc(void* ptr, size_t size);

void free(void* ptr, size_t size = 0, size_t alignment = 0);

size_t get_size(void* ptr);

}  // namespace bench
