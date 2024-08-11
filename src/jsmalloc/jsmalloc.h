#pragma once

#include <cstring>

#include "src/heap_interface.h"

namespace jsmalloc {

// Called before any allocations are made.
void initialize_heap(bench::Heap& heap);

void* malloc(size_t size);

void* calloc(size_t nmemb, size_t size);

void* realloc(void* ptr, size_t size);

void free(void* ptr);

}  // namespace jsmalloc
