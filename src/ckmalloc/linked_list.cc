#include "src/ckmalloc/linked_list.h"

#include "src/ckmalloc/util.h"

namespace ckmalloc {

void LinkedListNode::Remove() const {
  CK_ASSERT_EQ(next_->prev_, this);
  CK_ASSERT_EQ(prev_->next_, this);
  next_->prev_ = prev_;
  prev_->next_ = next_;
}

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

}  // namespace ckmalloc
