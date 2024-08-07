#pragma once

#include <cstring>
#include "src/heap_interface.h"

namespace jsmalloc {

// Called before any allocations are made.
void initialize_heap(bench::Heap& heap);

void* malloc(bench::Heap& heap, size_t size);

void* calloc(bench::Heap& heap, size_t nmemb, size_t size);

void* realloc(bench::Heap& heap, void* ptr, size_t size);

void free(void* ptr);

}  // namespace jsmalloc
