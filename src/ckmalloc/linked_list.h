#pragma once

#include <cstddef>
#include <type_traits>

#include "src/ckmalloc/util.h"

namespace ckmalloc {

class LinkedListNode {
  template <typename T>
  friend class LinkedList;

  template <typename T>
  friend class LinkedListIterator;

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
class LinkedListIterator {
  template <typename U>
  friend class LinkedList;

 public:
  using value_type = std::remove_reference_t<T>;
  using node_type = std::conditional_t<std::is_const_v<value_type>,
                                       const LinkedListNode, LinkedListNode>;

  LinkedListIterator(const LinkedListIterator&) = default;

  bool operator==(const LinkedListIterator& it) const {
    return node_ == it.node_;
  }
  bool operator!=(const LinkedListIterator& it) const {
    return !(*this == it);
  }

  value_type& operator*() const {
    return *static_cast<value_type*>(node_);
  }
  value_type* operator->() const {
    return static_cast<value_type*>(node_);
  }

  LinkedListIterator& operator++() {
    node_ = node_->next_;
    return *this;
  }
  LinkedListIterator operator++(int) {
    LinkedListIterator copy = *this;
    node_ = node_->next_;
    return copy;
  }

 private:
  explicit LinkedListIterator(node_type& node) : node_(&node) {}

  node_type* node_;
};

template <typename T>
class LinkedList {
 public:
  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using iterator = LinkedListIterator<reference>;
  using const_iterator = LinkedListIterator<const_reference>;

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

  bool Empty() const {
    return size_ == 0;
  }

  size_t Size() const {
    return size_;
  }

  iterator begin() {
    return LinkedListIterator<reference>(*sentinel_.next_);
  }

  const_iterator begin() const {
    return LinkedListIterator<const_reference>(*sentinel_.next_);
  }

  iterator end() {
    return LinkedListIterator<reference>(sentinel_);
  }

  const_iterator end() const {
    return LinkedListIterator<const_reference>(sentinel_);
  }

  void InsertFront(T* item) {
    item->InsertAfter(sentinel_);
    size_++;
  }

  void InsertBack(T* item) {
    item->InsertBefore(sentinel_);
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

  void InsertAfter(iterator it, T* item) {
    static_cast<LinkedListNode*>(item)->InsertAfter(*it.node_);
    size_++;
  }

  void Remove(T* item) {
    static_cast<LinkedListNode*>(item)->Remove();
    size_--;
  }

  void Remove(iterator it) {
    it.node_->Remove();
    size_--;
  }

 private:
  LinkedListNode sentinel_;
  size_t size_ = 0;
};

template <typename T1, typename T2>
bool operator==(const LinkedListIterator<T1>& it1,
                const LinkedListIterator<T2>& it2) {
  return it1.node_ == it2.node_;
}
template <typename T1, typename T2>
bool operator!=(const LinkedListIterator<T1>& it1,
                const LinkedListIterator<T2>& it2) {
  return !(it1 == it2);
}

}  // namespace ckmalloc
