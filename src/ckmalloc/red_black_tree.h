#pragma once

#include <cstddef>
#include <functional>

namespace ckmalloc {

class RbNode {
  template <typename T, typename Cmp>
  friend class RbTree;

  template <typename T>
  friend class RbTreeIterator;

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

  bool IsBlack() const {
    return !red_;
  }

 private:
  RbNode& operator=(const RbNode&) = default;
  RbNode& operator=(RbNode&&) = default;

  // Rotate left about `this`. Pass the right child of `this` if already loaded
  // into a variable.
  void RotateLeft(RbNode* right);

  // Rotate right about `this`. Pass the left child of `this` if already loaded
  // into a variable.
  void RotateRight(RbNode* left);

  // Equivalent to:
  // this->RotateLeft(right);
  // parent->RotateRight(right);
  //
  // `this` is the left child of parent, and right is the right child of `this`.
  void RotateLeftRight(RbNode* parent, RbNode* right);

  // Equivalent to:
  // this->RotateRight(left);
  // parent->RotateLeft(left);
  //
  // `this` is the right child of parent, and left is the left child of `this`.
  void RotateRightLeft(RbNode* parent, RbNode* left);

  // Inserts this node to the left of `node`, fixing the tree as necessary.
  void InsertLeft(RbNode* node, const RbNode* root);

  // Inserts this node to the right of `node`, fixing the tree as necessary.
  void InsertRight(RbNode* node, const RbNode* root);

  // Removes this node, fixing the tree as necessary.
  void Remove(const RbNode* root) const;

  RbNode* Left() {
    return left_;
  }

  RbNode* Right() {
    return right_;
  }

  RbNode* Parent() {
    return parent_;
  }

  void MakeRed() {
    red_ = true;
  }

  void MakeBlack() {
    red_ = false;
  }

  void SetLeft(RbNode* node);

  void SetRight(RbNode* node);

  void SetParentOf(const RbNode* node);

  // Detaches this RbNode from its parent, replacing it with `new_child`. Either
  // `parent_` or `new_child` may be null. This does not modify `this`.
  void DetachParent(RbNode* new_child) const;

  RbNode* Next() {
    return const_cast<RbNode*>(static_cast<const RbNode*>(this)->Next());
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

  RbNode* Prev() {
    return const_cast<RbNode*>(static_cast<const RbNode*>(this)->Prev());
  }

  const RbNode* Prev() const {
    if (left_ != nullptr) {
      return left_->RightmostChild();
    }

    const RbNode* node = this;
    const RbNode* prev = nullptr;
    while (node != nullptr && node->left_ == prev) {
      prev = node;
      node = node->parent_;
    }
    return node;
  }

  RbNode* LeftmostChild() {
    return left_ != nullptr ? left_->LeftmostChild() : this;
  }

  const RbNode* LeftmostChild() const {
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

  static void InsertFix(RbNode* node, const RbNode* root);

  // Fixes a node `node` which has a black height of 1 less than it should. The
  // subtree rooted at `node` should still be a valid red-black tree (except
  // `node` may be red).
  static void DeleteFix(RbNode* node, RbNode* parent, const RbNode* root);

  RbNode* left_ = nullptr;
  RbNode* right_ = nullptr;
  RbNode* parent_ = nullptr;
  // TODO: put this guy in parent_ or something.
  bool red_ = true;
};

template <typename T>
class RbTreeIterator {
  template <typename U, typename Cmp>
  friend class RbTree;

 public:
  using value_type = std::remove_reference_t<T>;
  using node_type =
      std::conditional_t<std::is_const_v<value_type>, const RbNode, RbNode>;

  RbTreeIterator(const RbTreeIterator&) = default;

  bool operator==(const RbTreeIterator& it) const {
    return node_ == it.node_;
  }
  bool operator!=(const RbTreeIterator& it) const {
    return !(*this == it);
  }

  value_type& operator*() const {
    return *static_cast<value_type*>(node_);
  }
  value_type* operator->() const {
    return static_cast<value_type*>(node_);
  }

  RbTreeIterator& operator++() {
    node_ = node_->Next();
    return *this;
  }
  RbTreeIterator operator++(int) {
    RbTreeIterator copy = *this;
    node_ = node_->Next();
    return copy;
  }

 private:
  explicit RbTreeIterator(node_type& node) : node_(&node) {}

  node_type* node_;
};

template <typename T, typename Cmp = std::less<T>>
class RbTree {
  friend class RedBlackTreeTest;

 public:
  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using iterator = RbTreeIterator<reference>;
  using const_iterator = RbTreeIterator<const_reference>;

  RbTree() = default;

  // Disallow copy/move construction/assignment, since tree nodes will point
  // back to root, which lives in this class.
  RbTree(const RbTree<T, Cmp>&) = delete;
  RbTree(RbTree<T, Cmp>&&) = delete;
  RbTree<T, Cmp>& operator=(const RbTree<T, Cmp>&) = delete;
  RbTree<T, Cmp>& operator=(RbTree<T, Cmp>&&) = delete;

  bool Empty() const {
    return size_ == 0;
  }

  size_t Size() const {
    return size_;
  }

  iterator begin() {
    return RbTreeIterator<reference>(*root_.LeftmostChild());
  }

  const_iterator begin() const {
    return RbTreeIterator<const_reference>(*root_.LeftmostChild());
  }

  iterator end() {
    return RbTreeIterator<reference>(root_);
  }

  const_iterator end() const {
    return RbTreeIterator<const_reference>(root_);
  }

  T* Next(T* item) {
    RbNode* next = static_cast<RbNode*>(item)->Next();
    return next != RootSentinel() ? static_cast<T*>(next) : nullptr;
  }

  const T* Next(const T* item) const {
    const RbNode* next = static_cast<const RbNode*>(item)->Next();
    return next != RootSentinel() ? static_cast<const T*>(next) : nullptr;
  }

  T* Prev(T* item) {
    RbNode* prev = static_cast<RbNode*>(item)->Prev();
    return prev != nullptr ? static_cast<T*>(prev) : nullptr;
  }

  const T* Prev(const T* item) const {
    const RbNode* prev = static_cast<const RbNode*>(item)->Prev();
    return prev != nullptr ? static_cast<const T*>(prev) : nullptr;
  }

  void Insert(T* item) {
    if (Root() == nullptr) {
      auto* node = static_cast<RbNode*>(item);
      node->Reset();
      root_.SetLeft(node);
      size_++;
      return;
    }

    RbNode* parent = Root();
    for (RbNode* node; (node = Cmp{}(*item, *static_cast<T*>(parent))
                                   ? parent->left_
                                   : parent->right_) != nullptr;
         parent = node)
      ;

    if (Cmp{}(*item, *static_cast<T*>(parent))) {
      item->RbNode::InsertLeft(parent, RootSentinel());
    } else {
      item->RbNode::InsertRight(parent, RootSentinel());
    }
    size_++;
  }

  void Remove(T* item) {
    item->RbNode::Remove(RootSentinel());
    size_--;
  }

  // Returns the lowest-valued element in the tree that `AtLeast`() is true
  // for.
  template <typename AtLeast>
  T* LowerBound(AtLeast at_least) {
    RbNode* node = Root();
    RbNode* smallest = nullptr;
    while (node != nullptr) {
      if (at_least(*static_cast<const T*>(node))) {
        smallest = node;
        node = node->left_;
      } else {
        node = node->right_;
      }
    }

    return smallest != nullptr ? static_cast<T*>(smallest) : nullptr;
  }

 private:
  RbNode* Root() {
    return root_.Left();
  }

  const RbNode* RootSentinel() const {
    return &root_;
  }

  const RbNode* Root() const {
    return root_.Left();
  }

  RbNode root_;
  size_t size_ = 0;
};

}  // namespace ckmalloc
