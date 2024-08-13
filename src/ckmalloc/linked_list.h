#pragma once

#include <type_traits>

#include "src/ckmalloc/util.h"

namespace ckmalloc {

class LinkedListNode {
  template <typename T>
  friend class LinkedList;

  template <typename T>
  friend class LinkedListIterator;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const class Block& block);

 public:
  LinkedListNode() = default;

  // Nodes cannot be moved/copied.
  LinkedListNode(const LinkedListNode&) = delete;
  LinkedListNode(LinkedListNode&&) = delete;

  // Linked list nodes can remove themselves.
  void Remove() const;

 private:
  LinkedListNode(LinkedListNode* next, LinkedListNode* prev);

  const LinkedListNode* Next() const {
    return next_;
  }

  const LinkedListNode* Prev() const {
    return prev_;
  }

  void InsertAfter(LinkedListNode& node);

  void InsertBefore(LinkedListNode& node);

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
  using difference_type = size_t;

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
    return sentinel_.next_ == &sentinel_;
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
  }

  void InsertBack(T* item) {
    item->InsertBefore(sentinel_);
  }

  T* Front() const {
    LinkedListNode* first = sentinel_.next_;
    return first != &sentinel_ ? static_cast<T*>(first) : nullptr;
  }

  T* PopFront() {
    LinkedListNode* first = sentinel_.next_;
    CK_ASSERT_NE(first, &sentinel_);
    first->Remove();
    return static_cast<T*>(first);
  }

  T* Back() const {
    LinkedListNode* last = sentinel_.prev_;
    return last != &sentinel_ ? static_cast<T*>(last) : nullptr;
  }

  T* PopBack() {
    LinkedListNode* last = sentinel_.prev_;
    CK_ASSERT_NE(last, &sentinel_);
    last->Remove();
    return static_cast<T*>(last);
  }

  void InsertAfter(iterator it, T* item) {
    static_cast<LinkedListNode*>(item)->InsertAfter(*it.node_);
  }

  void Remove(T* item) {
    static_cast<LinkedListNode*>(item)->Remove();
  }

  void Remove(iterator it) {
    it.node_->Remove();
  }

 private:
  LinkedListNode sentinel_;
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
