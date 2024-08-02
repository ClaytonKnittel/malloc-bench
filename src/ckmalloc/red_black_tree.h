#pragma once

#include <cstddef>
#include <functional>
#include <optional>

#include "src/ckmalloc/util.h"

namespace ckmalloc {

class RbNode {
  template <typename T, typename Cmp>
  friend class RbTree;

 public:
  RbNode() = default;

  // Nodes cannot be moved/copied.
  RbNode(const RbNode&) = delete;
  RbNode(RbNode&&) = delete;

  const RbNode* Left() const {
    return left_;
  }

  const RbNode* Right() const {
    return right_;
  }

  const RbNode* Parent() const {
    return parent_;
  }

  bool IsRed() const {
    return red_;
  }

  const RbNode* Next() const {
    if (right_ != nullptr) {
      return right_->LeftmostChild();
    }

    const RbNode* node = this;
    const RbNode* prev = nullptr;
    while (node != nullptr && node->right_ == prev) {
      prev = node;
      node = node->parent_;
    }
    return node;
  }

  const RbNode* LeftmostChild() const {
    return left_ != nullptr ? left_->LeftmostChild() : this;
  }

 private:
  RbNode& operator=(const RbNode&) = default;
  RbNode& operator=(RbNode&&) = default;

  // Inserts this node to the left of `node`, returning the current root after
  // the operation is complete.
  std::optional<RbNode*> InsertLeft(RbNode* node);

  // Inserts this node to the right of `node`, returning the current root
  // after the operation is complete.
  std::optional<RbNode*> InsertRight(RbNode* node);

  // Removes this node, returning the new root of the tree.
  std::optional<RbNode*> Remove();

  void MakeRed() {
    red_ = true;
  }

  void MakeBlack() {
    red_ = false;
  }

  void SetLeft(RbNode* node);

  void SetRight(RbNode* node);

  void SetParentOf(RbNode* node);

  // Detaches this RbNode from its parent, replacing it with `new_child`. Either
  // `parent_` or `new_child` may be null.
  void DetachParent(RbNode* new_child);

  RbNode* LeftmostChild() {
    return left_ != nullptr ? left_->LeftmostChild() : this;
  }

  RbNode* RightmostChild() {
    return right_ != nullptr ? right_->RightmostChild() : this;
  }

  void Reset() {
    left_ = nullptr;
    right_ = nullptr;
    parent_ = nullptr;
    red_ = false;
  }

  static std::optional<RbNode*> InsertFix(RbNode* node);

  static std::optional<RbNode*> DeleteFix(RbNode* node);

  RbNode* left_ = nullptr;
  RbNode* right_ = nullptr;
  RbNode* parent_ = nullptr;
  bool red_ = true;
};

template <typename T, typename Cmp = std::less<T>>
class RbTree {
 public:
  const RbNode* Root() const {
    return root_;
  }

  size_t Size() const {
    return size_;
  }

  void Insert(T* item) {
    if (root_ == nullptr) {
      root_ = static_cast<RbNode*>(item);
      root_->Reset();
      size_++;
      return;
    }

    RbNode* parent = root_;
    for (RbNode* node; (node = Cmp{}(*item, *static_cast<T*>(parent))
                                   ? parent->left_
                                   : parent->right_) != nullptr;
         parent = node)
      ;

    std::optional<RbNode*> new_root;
    if (Cmp{}(*item, *static_cast<T*>(parent))) {
      new_root = item->RbNode::InsertLeft(parent);
    } else {
      new_root = item->RbNode::InsertRight(parent);
    }

    if (new_root.has_value()) {
      root_ = new_root.value();
    }
    size_++;
  }

  void Remove(T* item) {
    std::optional<RbNode*> new_root = item->RbNode::Remove();
    if (new_root.has_value()) {
      root_ = new_root.value();
    }
    size_--;
  }

  // Returns the lowest-valued element in the tree that `AtLeast`() is true
  // for.
  template <typename AtLeast>
  T* LowerBound(AtLeast at_least) {
    RbNode* node = root_;
    RbNode* smallest = nullptr;
    while (node != nullptr) {
      if (at_least(*static_cast<T*>(node))) {
        smallest = node;
        node = node->left_;
      } else {
        node = node->right_;
      }
    }

    return smallest != nullptr ? static_cast<T*>(smallest) : nullptr;
  }

 private:
  RbNode* root_ = nullptr;
  size_t size_ = 0;
};

}  // namespace ckmalloc
