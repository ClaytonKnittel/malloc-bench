#include "src/ckmalloc/red_black_tree.h"

#include <iomanip>
#include <optional>

#include "src/ckmalloc/util.h"

namespace ckmalloc {

void RbNode::RotateLeft(RbNode* right) {
  CK_ASSERT(right == right_);
  this->SetRight(right->Left());
  right->SetParentOf(this);
  this->parent_ = right;
  right->left_ = this;
}

void RbNode::RotateRight(RbNode* left) {
  CK_ASSERT(left == left_);
  this->SetLeft(left->Right());
  left->SetParentOf(this);
  this->parent_ = left;
  left->right_ = this;
}

void RbNode::RotateRightLeft(RbNode* parent, RbNode* right) {
  CK_ASSERT(parent == parent_);
  CK_ASSERT(right == right_);
  this->SetRight(right->Left());
  parent->SetLeft(right->Right());
  right->SetParentOf(parent);
  right->SetLeft(this);
  right->SetRight(parent);
}

void RbNode::RotateLeftRight(RbNode* parent, RbNode* left) {
  CK_ASSERT(parent == parent_);
  CK_ASSERT(left == left_);
  this->SetLeft(left->Right());
  parent->SetRight(left->Left());
  left->SetParentOf(parent);
  left->SetRight(this);
  left->SetLeft(parent);
}

std::optional<RbNode*> RbNode::InsertLeft(RbNode* node) {
  CK_ASSERT(node->left_ == nullptr);
  node->left_ = this;
  this->parent_ = node;
  this->MakeRed();
  return InsertFix(this);
}

std::optional<RbNode*> RbNode::InsertRight(RbNode* node) {
  CK_ASSERT(node->right_ == nullptr);
  node->right_ = this;
  this->parent_ = node;
  this->MakeRed();
  return InsertFix(this);
}

std::optional<RbNode*> RbNode::Remove() const {
  RbNode* successor;
  RbNode* node_to_fix;
  if (left_ == nullptr) {
    successor = right_;
    node_to_fix = red_ ? nullptr : (right_ != nullptr ? right_ : parent_);
    DetachParent(right_);
  } else if (right_ == nullptr) {
    successor = left_;
    node_to_fix = red_ ? nullptr : left_;
    DetachParent(left_);
  } else {
    RbNode* successor = left_->RightmostChild();
    // If the successor was black, we will fix from it's left child. If it had
    // no left child, fix from its parent.
    node_to_fix = successor->red_
                      ? nullptr
                      : (successor->left_ != nullptr ? successor->left_
                                                     : successor->parent_);
    // successor does not have a right child. Detach it from its parent and
    // replace it with its left (only) child.
    successor->DetachParent(successor->left_);

    // Replace this node with the successor.
    successor->SetLeft(left_);
    successor->SetRight(right_);
    successor->SetParentOf(this);
    successor->red_ = red_;
  }

  // We have already replaced `this` with `successor`. If `this` was previously
  // the root of the tree, we will need to return the updated root.
  std::optional<RbNode*> new_root =
      parent_ == nullptr ? std::optional<RbNode*>(successor) : std::nullopt;
  if (node_to_fix == nullptr) {
    return new_root;
  }

  // If `DeleteFix` returns a new root, return that. Otherwise, return
  // `successor` if `this` was previously the root, else root has not changed.
  return OptionalOr(DeleteFix(node_to_fix), std::move(new_root));
}

void RbNode::SetLeft(RbNode* node) {
  left_ = node;
  if (node != nullptr) {
    node->parent_ = this;
  }
}

void RbNode::SetRight(RbNode* node) {
  right_ = node;
  if (node != nullptr) {
    node->parent_ = this;
  }
}

void RbNode::SetParentOf(const RbNode* node) {
  parent_ = node->parent_;
  if (parent_ != nullptr) {
    if (parent_->left_ == node) {
      parent_->left_ = this;
    } else {
      parent_->right_ = this;
    }
  }
}

void RbNode::DetachParent(RbNode* new_child) const {
  if (new_child != nullptr) {
    new_child->parent_ = parent_;
  }
  if (parent_ != nullptr) {
    if (parent_->left_ == this) {
      parent_->left_ = new_child;
    } else {
      parent_->right_ = new_child;
    }
  }
}

namespace {
struct Element : public RbNode {
  int val;
};

std::ostream& operator<<(std::ostream& ostr, const Element& element) {
  return ostr << element.val << (element.IsRed() ? " (r)" : " (b)");
}

bool operator<(const Element& e1, const Element& e2) {
  return e1.val < e2.val;
}

void PrintNode(const RbNode* node, int depth) {
  if (node == nullptr) {
    return;
  }

  std::cout << std::setw(2 * depth) << "" << *static_cast<const Element*>(node)
            << " : ";
  if (node->Left() != nullptr) {
    std::cout << "l:" << static_cast<const Element*>(node->Left())->val << " ";
  }
  if (node->Right() != nullptr) {
    std::cout << "r:" << static_cast<const Element*>(node->Right())->val << " ";
  }
  if (node->Parent() != nullptr) {
    std::cout << "p:" << static_cast<const Element*>(node->Parent())->val
              << " ";
  }
  std::cout << std::endl;
  PrintNode(node->Left(), depth + 1);
  PrintNode(node->Right(), depth + 1);
}

void Print(const RbNode* any_node) {
  while (any_node->Parent() != nullptr) {
    any_node = any_node->Parent();
  }
  PrintNode(any_node, 0);
}

}  // namespace

/* static */
std::optional<RbNode*> RbNode::InsertFix(RbNode* n) {
  RbNode* p;
  while ((p = n->parent_) != nullptr && p->IsRed()) {
#define FIX_CHILD(dir, opp)         \
  RbNode* a = gp->opp();            \
  if (a != nullptr && a->IsRed()) { \
    p->MakeBlack();                 \
    a->MakeBlack();                 \
    gp->MakeRed();                  \
    n = gp;                         \
  } else if (n == p->dir()) {       \
    p->MakeBlack();                 \
    gp->MakeRed();                  \
    gp->Rotate##opp(p);             \
    n = p;                          \
  } else {                          \
    n->MakeBlack();                 \
    gp->MakeRed();                  \
    p->Rotate##opp##dir(gp, n);     \
    p = n->parent_;                 \
    break;                          \
  }

    RbNode* gp = p->parent_;
    if (p == gp->Left()) {
      FIX_CHILD(Left, Right);
    } else /* p == gp->Right() */ {
      FIX_CHILD(Right, Left);
    }

#undef FIX_CHILD
  }

  if (p != nullptr) {
    return std::nullopt;
  }

  n->MakeBlack();
  return n;
}

std::optional<RbNode*> RbNode::DeleteFix(RbNode* n) {
  RbNode* p;
  while ((p = n->Parent()) != nullptr && n->IsBlack()) {
#define FIX_CHILD(dir, opp)                                                \
  RbNode* s = p->opp();                                                    \
  CK_ASSERT(s != nullptr);                                                 \
  if (s->IsRed()) {                                                        \
    p->MakeRed();                                                          \
    s->MakeBlack();                                                        \
    p->Rotate##dir(s);                                                     \
  }                                                                        \
  /* Since `n` is double black, `s`'s black depth must be at least two, */ \
  /* so it must have left and right children. */                           \
  /* NOLINTNEXTLINE(readability-simplify-boolean-expr) */                  \
  CK_ASSERT(s->Left() != nullptr && s->Right() != nullptr);                \
  if (s->dir()->IsBlack() && s->opp()->IsBlack()) {                        \
    s->MakeRed();                                                          \
    n = p;                                                                 \
  } else if (s->opp()->IsRed()) {                                          \
    s->red_ = p->red_;                                                     \
    p->MakeBlack();                                                        \
    p->Rotate##dir(s);                                                     \
    break;                                                                 \
  } else /* s->dir()->IsRed() */ {                                         \
    RbNode* sd = s->dir();                                                 \
    sd->red_ = p->red_;                                                    \
    p->MakeBlack();                                                        \
    s->Rotate##opp##dir(p, sd);                                            \
    break;                                                                 \
  }

    if (n == p->Left()) {
      FIX_CHILD(Left, Right);
    } else /* n == p->Right() */ {
      FIX_CHILD(Right, Left);
    }

#undef FIX_CHILD
  }

  // If we landed on a red node, we can color it black and that will fix the
  // black defecit. If we happened to land on the root, then we need to color
  // it black anyway, so this coincidentally covers both cases.
  if (n->IsRed()) {
    n->MakeBlack();
  }

  return p != nullptr ? std::optional<RbNode*>(p) : std::nullopt;
}

}  // namespace ckmalloc
