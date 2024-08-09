#pragma once

#include <cstddef>

#include "src/ckmalloc/util.h"
namespace ckmalloc {

class LinkedListNode {
  template <typename T>
  friend class LinkedList;

 public:
  LinkedListNode() = default;

  // Nodes cannot be moved/copied.
  LinkedListNode(const LinkedListNode&) = delete;
  LinkedListNode(LinkedListNode&&) = delete;

  const LinkedListNode* Next() const {
    return next_;
  }

  const LinkedListNode* Prev() const {
    return prev_;
  }

 private:
  LinkedListNode(LinkedListNode* next, LinkedListNode* prev);

  void InsertAfter(LinkedListNode& node);

  void InsertBefore(LinkedListNode& node);

  void Remove() const;

  LinkedListNode* next_ = nullptr;
  LinkedListNode* prev_ = nullptr;
};

template <typename T>
class LinkedList {
 public:
  LinkedList() : sentinel_(&sentinel_, &sentinel_) {}

  // Disallow copy/move construction/assignment, since list nodes will point
  // back to root, which lives in this class.
  LinkedList(const LinkedList<T>&) = delete;
  LinkedList(LinkedList<T>&&) = delete;
  LinkedList<T>& operator=(const LinkedList<T>&) = delete;
  LinkedList<T>& operator=(LinkedList<T>&&) = delete;

  const LinkedListNode* Sentinel() const {
    return &sentinel_;
  }

  size_t Size() const {
    return size_;
  }

  class Iterator {
    friend LinkedList;

   public:
    Iterator(const Iterator&) = default;

    bool operator==(const Iterator& it) const {
      return node_ == it.node_;
    }
    bool operator!=(const Iterator& it) const {
      return !(*this == it);
    }

    T* operator*() const {
      return static_cast<T*>(node_);
    }

    Iterator operator++() {
      node_ = node_->next_;
      return *this;
    }
    Iterator operator++(int) {
      Iterator copy = *this;
      node_ = node_->next_;
      return copy;
    }

   private:
    explicit Iterator(LinkedListNode* node) : node_(node) {}

    LinkedListNode* node_;
  };

  Iterator begin() {
    return Iterator(Front());
  }

  Iterator end() {
    return Iterator(&sentinel_);
  }

  void InsertFront(T* item) {
    item->InsertAfter(&sentinel_);
    size_++;
  }

  void InsertBack(T* item) {
    item->InsertBefore(&sentinel_);
    size_++;
  }

  T* Front() const {
    LinkedListNode* first = sentinel_.next_;
    return first != &sentinel_ ? static_cast<T*>(first) : nullptr;
  }

  T* PopFront() {
    LinkedListNode* first = sentinel_.next_;
    CK_ASSERT(first != &sentinel_);
    first->Remove();
    size_--;
    return static_cast<T*>(first);
  }

  T* Back() const {
    LinkedListNode* last = sentinel_.prev_;
    return last != &sentinel_ ? static_cast<T*>(last) : nullptr;
  }

  T* PopBack() {
    LinkedListNode* last = sentinel_.prev_;
    CK_ASSERT(last != &sentinel_);
    last->Remove();
    size_--;
    return static_cast<T*>(last);
  }

  void InsertAfter(Iterator it, T* item) {
    static_cast<LinkedListNode*>(item)->InsertAfter(it.node_);
    size_++;
  }

  void Remove(Iterator it) {
    it.node_->Remove();
    size_--;
  }

 private:
  LinkedListNode sentinel_;
  size_t size_ = 0;
};

}  // namespace ckmalloc
