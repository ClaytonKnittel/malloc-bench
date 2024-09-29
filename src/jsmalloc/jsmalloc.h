#pragma once

#include <cstddef>
#include <cstring>

#include "src/heap_factory.h"

namespace jsmalloc {

// Called before any allocations are made.
void initialize_heap(bench::HeapFactory& heap);

void* malloc(size_t size, size_t alignment = 0);

void* calloc(size_t nmemb, size_t size);

void* realloc(void* ptr, size_t size);

void free(void* ptr, size_t size = 0, size_t alignment = 0);

}  // namespace jsmalloc
