#pragma once

#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {

template <typename Item, typename Accessor>
class IntrusiveLinkedList {
 public:
  class Node {
   public:
    Node() : Node(nullptr, nullptr) {}

   private:
    friend IntrusiveLinkedList<Item, Accessor>;

    Node(Node* prev, Node* next) : next_(next), prev_(prev) {}

    /** Inserts `this` after `node`. */
    void insert_after(Node& node) {
      next_ = node.next_;
      prev_ = &node;
      DCHECK_NON_NULL(node.next_)->prev_ = this;
      node.next_ = this;
    }

    /** Inserts `this` before `node`. */
    void insert_before(Node& node) {
      next_ = &node;
      prev_ = node.prev_;
      node.prev_ = this;
      DCHECK_NON_NULL(prev_)->next_ = this;
    }

    void remove() {
      DCHECK_NON_NULL(this->prev_)->next_ = this->next_;
      DCHECK_NON_NULL(this->next_)->prev_ = this->prev_;
      this->prev_ = nullptr;
      this->next_ = nullptr;
    }

    bool linked() const {
      return next_ != nullptr;
    }

    Node* next_ = nullptr;
    Node* prev_ = nullptr;
  };

  explicit IntrusiveLinkedList() : head_(&head_, &head_) {}

  class Iterator {
    friend IntrusiveLinkedList;

   public:
    Iterator& operator++() {
      curr_ = curr_->next_;
      return *this;
    }

    Iterator& operator--() {
      curr_ = curr_->prev_;
      return *this;
    }

    bool operator==(const Iterator& other) {
      return curr_ == other.curr_;
    }

    bool operator!=(const Iterator& other) {
      return !(*this == other);
    }

    Item& operator*() {
      return *Accessor::GetItem(curr_);
    }

   private:
    explicit Iterator(Node& node) : curr_(&node) {}

    Node* curr_;
  };

  Iterator begin() {
    return Iterator(*head_.next_);
  }

  Iterator end() {
    return Iterator(head_);
  }

  /**
   * Returns whether `el` is in this list.
   *
   * Assumes that `el` _could_ only be in this list.
   */
  bool empty() const {
    return head_.next_ == &head_;
  }

  /**
   * Returns whether `el` is linked in some list.
   */
  static bool is_linked(Item& el) {
    return Accessor::GetNode(&el)->linked();
  }

  /**
   * Removes `el` from this list.
   */
  static void unlink(Item& el) {
    Accessor::GetNode(&el)->remove();
  }

  void insert_back(Item& el) {
    Accessor::GetNode(&el)->insert_before(head_);
  }

  void insert_front(Item& el) {
    Accessor::GetNode(&el)->insert_after(head_);
  }

  Item* front() {
    Item* item =  Accessor::GetItem(head_.next_);
    return empty() ? nullptr : item;
  }

  Item* back() {
    Item* item =  Accessor::GetItem(head_.prev_);
    return empty() ? nullptr : item;
  }

  size_t DebugSize() {
    size_t size = 0;
    for (auto it = this->begin(); it != this->end(); ++it) {
      size++;
    }
    return size;
  }

 private:
  Node head_;
};

}  // namespace jsmalloc
