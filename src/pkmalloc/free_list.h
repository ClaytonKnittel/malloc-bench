#pragma once

#include "src/pkmalloc/allocated_block.h"
#include "src/pkmalloc/block.h"
#include "src/pkmalloc/free_block.h"

struct ListNode {
  ListNode* left_;
  Block* block;
  ListNode* right_;
  uint8_t size_;
  bool free_;
} list_node;

// change this ^ later, bad because it takes up so much space
// go back to using bit stuff to be most efficient

class FreeList {
 public:
  // initializes heap metedata structure and returns a pointer to it on the heap
  ListNode* create_free_list_structure(FreeBlock* first_alloc_mem, size_t size);

  // changes heap metadata structure to align with current state of heap
  ListNode* edit_free_list_structure(ListNode* current_block);

  // allocate more memory for heap metadata structure when it runs out of room
  ListNode* realloc_free_list_structure();

  // doubly linked list

 private:
  // How to store this info? maybe make a meta struct item
  FreeBlock* begin_;
  size_t heap_metadata_size_;
};