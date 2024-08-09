#include "src/ckmalloc/linked_list.h"

namespace ckmalloc {

LinkedListNode::LinkedListNode(LinkedListNode* next, LinkedListNode* prev)
    : next_(next), prev_(prev) {}

void LinkedListNode::InsertAfter(LinkedListNode& node) {
  next_ = node.next_;
  prev_ = &node;
  next_->prev_ = this;
  node.next_ = this;
}

void LinkedListNode::InsertBefore(LinkedListNode& node) {
  next_ = &node;
  prev_ = node.prev_;
  prev_->next_ = this;
  node.prev_ = this;
}

void LinkedListNode::Remove() const {
  next_->prev_ = prev_;
  prev_->next_ = next_;
}

}  // namespace ckmalloc
