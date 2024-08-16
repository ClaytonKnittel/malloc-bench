#include "src/pkmalloc/free_list.h"

ListNode* FreeList::create_free_list_structure(FreeBlock* first_alloc_mem,
                                               size_t size) {
  ListNode* new_node;
  new_node->left_ = nullptr;
  new_node->right_ = nullptr;
  new_node->free_ = false;
  new_node->size_ = size;
  // allocate some amount of space at a certain point on the heap to store this
  // info keep track of the end so that it doesn't interfere with availble heap
  // space for user
  return new_node;
}
// have a pointer to this structure
// which will be stored on the heap
// this will hold all the free blocks
// and their size

ListNode* FreeList::edit_free_list_structure(ListNode* current_block) {}

ListNode* FreeList::realloc_free_list_structure() {
  // when heap metadata exceeds current allocated size for it, must reallocate
  // it by either extending out if adjacent memory is free or moving into a
  // bigger free block or sbrk
}