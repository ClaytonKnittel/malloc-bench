#pragma once

#include <cstddef>
#include <cstdint>

namespace jsmalloc {

template <class T, class M>
static inline constexpr ptrdiff_t offset_of(const M T::*member) {
  return reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<T*>(0)->*member));
}

template <class T, class M>
static inline constexpr T* owner_of(const M* ptr, const M T::*member) {
  return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) -
                              offset_of(member));
}

template <typename T>
class IntrusiveLinkedList {
 public:
  class Node {
    friend IntrusiveLinkedList;

   public:
    Node() : Node(nullptr, nullptr) {}

   private:
    Node(Node* prev, Node* next) : next_(next), prev_(prev) {}

    void insert_the_argument_after_me(Node& node) {
      next_ = &node;
      prev_ = node.prev_;
      prev_->next_ = this;
      node.prev_ = this;
    }

    void remove() {
      this->prev_->next_ = this->next_;
      this->next_->prev_ = this->prev_;
      this->prev_ = nullptr;
      this->next_ = nullptr;
    }

    T* item(Node T::*node_field) const {
      return owner_of(this, node_field);
    }

    Node* next_ = nullptr;
    Node* prev_ = nullptr;
  };

  explicit IntrusiveLinkedList(Node T::*node_field)
      : node_field_(node_field), head_(&head_, &head_) {}

  class Iterator {
    friend IntrusiveLinkedList;

   public:
    Iterator& operator++() {
      curr_ = curr_->next_;
      return *this;
    }

    bool operator==(const Iterator& other) {
      return curr_ == other.curr_;
    }

    bool operator!=(const Iterator& other) {
      return !(*this == other);
    }

    T& operator*() {
      return *curr_->item(node_field_);
    }

   private:
    explicit Iterator(Node& node, Node T::*node_field)
        : curr_(&node), node_field_(node_field) {}

    Node* curr_;
    Node T::*node_field_;
  };

  size_t size() const {
    return size_;
  }

  Iterator begin() {
    return Iterator(*head_.next_, node_field_);
  }

  Iterator end() {
    return Iterator(head_, node_field_);
  }

  void insert_back(T& el) {
    (el.*node_field_).insert_the_argument_after_me(head_);
    size_++;
  }

  void insert_front(T& el) {
    head_.insert_the_argument_after_me(el.*node_field_);
    size_++;
  }

  void remove(T& el) {
    (el.*node_field_).remove();
    size_--;
  }

  T* front() {
    if (size_ == 0) {
      return nullptr;
    }
    return head_.next_->item(node_field_);
  }

  T* back() {
    if (size_ == 0) {
      return nullptr;
    }
    return head_.prev_->item(node_field_);
  }

  Node T::*node_field_;
  Node head_;
  size_t size_ = 0;
};

}  // namespace jsmalloc
