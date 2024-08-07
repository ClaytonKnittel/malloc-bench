#pragma once

#include <cstddef>
namespace jsmalloc {

template <typename T>
class IntrusiveLinkedList {
 public:
  class Item {
    friend IntrusiveLinkedList;

   public:
    Item() : Item(nullptr, nullptr) {}

   private:
    Item(Item* prev, Item* next) : next_(next), prev_(prev) {}

    void InsertTheArgumentAfterMe(Item& item) {
      next_ = &item;
      prev_ = item.prev_;
      prev_->next_ = this;
      item.prev_ = this;
    }

    Item* next_ = nullptr;
    Item* prev_ = nullptr;
  };

  IntrusiveLinkedList() : head_(&head_, &head_) {}

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
      return *static_cast<T*>(curr_);
    }

   private:
    explicit Iterator(Item& item) : curr_(&item) {}

    Item* curr_;
  };

  size_t size() const {
    return size_;
  }

  Iterator begin() {
    return Iterator(*head_.next_);
  }

  Iterator end() {
    return Iterator(head_);
  }

  void insert_back(T& el) {
    auto& item = static_cast<Item&>(el);
    item.InsertTheArgumentAfterMe(head_);
    size_++;
  }

  void insert_front(T& el) {
    auto& item = static_cast<Item&>(el);
    head_.InsertTheArgumentAfterMe(item);
    size_++;
  }

  T* front() {
    if (size_ == 0) {
      return nullptr;
    }
    return static_cast<T*>(head_.next_);
  }

  T* back() {
    if (size_ == 0) {
      return nullptr;
    }
    return static_cast<T*>(head_.prev_);
  }

  Item head_;
  size_t size_ = 0;
};

}  // namespace jsmalloc