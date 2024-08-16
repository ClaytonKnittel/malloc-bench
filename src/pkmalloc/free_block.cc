#include "src/pkmalloc/free_block.h"

#include "src/pkmalloc/free_list.h"

void FreeBlock::coalesce(ListNode* current) {
  // check in both directions for free blocks, if free, combine
  ListNode* left = current->left_;
  ListNode* right = current->right_;
  if (left != nullptr) {
    if (left->free_) {
      current = combine(current, left);
    }
  }
  if (right != nullptr) {
    if (right->free_) {
      current = combine(current, right);
    }
  }
  // update current free list to make these blocks only one in the linked list?
  // return current;
}

ListNode* FreeBlock::combine(ListNode* left_block, ListNode* right_block) {
  // merge two free blocks
  uint8_t size = left_block->size_;
  size += right_block->size_;

  // create new block that starts at left
  ListNode new_node;
  new_node.left_ = left_block->left_;
  new_node.right_ = right_block->right_;
  new_node.free_ = true;
  new_node.size_ = size;

  left_block->right_ = &new_node;
  right_block->left_ = &new_node;
}