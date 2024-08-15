#include "src/pkmalloc/free_list.h"

FreeList* FreeList::create_free_list_structure() {
  // allocate some amount of space at a certain point on the heap to store this
  // info keep track of the end so that it doesn't interfere with availble heap
  // space for user
  return nullptr;
}
// have a pointer to this structure
// which will be stored on the heap
// this will hold all the free blocks
// and their size

FreeList* FreeList::edit_free_list_structure() {}

FreeList* FreeList::realloc_free_list_structure() {
  // when heap metadata exceeds current allocated size for it, must reallocate
  // it by either extending out if adjacent memory is free or moving into a
  // bigger free block or sbrk
}