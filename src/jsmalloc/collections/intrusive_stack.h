#pragma once

#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {

template <typename Item, typename Accessor>
class IntrusiveStack {
 public:
  class Node {
   public:
    Node() : Node(nullptr) {}

   private:
    friend IntrusiveStack<Item, Accessor>;

    explicit Node(Node* next) : next_(next) {}

    Node* next_ = nullptr;
  };

  bool empty() const {
    return head_.next_ == nullptr;
  }

  void push(Item& el) {
    Node* node = Accessor::GetNode(&el);
    node->next_ = head_.next_;
    head_.next_ = node;
  }

  Item* peek() {
    DCHECK_FALSE(empty());
    return Accessor::GetItem(head_.next_);
  }

  Item* pop() {
    DCHECK_FALSE(empty());
    Item* el = Accessor::GetItem(head_.next_);
    head_.next_ = head_.next_->next_;
    return el;
  }

 private:
  Node head_;
};

}  // namespace jsmalloc
