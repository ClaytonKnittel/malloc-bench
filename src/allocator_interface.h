#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include "src/heap_factory.h"

namespace bench {

// calls heap factory, sets global_heap_start_pointer and heap_start_pointer
void* initialize_heap(HeapFactory& heap_factory);

void* malloc(size_t size, size_t alignment = 0);

void* calloc(size_t nmemb, size_t size);

void* realloc(void* ptr, size_t size);

void free(void* ptr, size_t size_hint = 0, size_t alignment_hint = 0);

size_t get_size(void* ptr);

}  // namespace bench
