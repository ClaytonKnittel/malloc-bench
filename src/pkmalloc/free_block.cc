#include "src/pkmalloc/free_block.h"

#include "src/pkmalloc/free_list.h"

void FreeBlock::coalesce(FreeList* current) {
  // check in both directions for free blocks, if free, combine
  FreeList* left = current->left;
  FreeList* right = current->right;
  if (left != nullptr) {
    if (IsFree(left)) {
      current = combine(current, left);
    }
  }
  if (right != nullptr) {
    if (IsFree(right)) {
      current = combine(current, right);
    }
  }
  // update current free list to make these blocks only one in the linked list?
  // return current;
}

void FreeBlock::combine(FreeList* left, FreeList* right) {
  // merge two free blocks
}