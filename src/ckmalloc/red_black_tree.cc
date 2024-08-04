#include "src/ckmalloc/red_black_tree.h"

#include <optional>

#include "src/ckmalloc/util.h"

namespace ckmalloc {

namespace {

bool IsRedPtr(const RbNode* node) {
  return node != nullptr && node->IsRed();
}

bool IsBlackPtr(const RbNode* node) {
  return node == nullptr || node->IsBlack();
}

}  // namespace

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

void RbNode::RotateLeftRight(RbNode* parent, RbNode* right) {
  CK_ASSERT(parent == parent_);
  CK_ASSERT(parent->left_ == this);
  CK_ASSERT(right == right_);
  this->SetRight(right->Left());
  parent->SetLeft(right->Right());
  right->SetParentOf(parent);
  right->SetLeft(this);
  right->SetRight(parent);
}

void RbNode::RotateRightLeft(RbNode* parent, RbNode* left) {
  CK_ASSERT(parent == parent_);
  CK_ASSERT(parent->right_ == this);
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
  // The node which will be replacing `this`'s place in the tree.
  RbNode* replacer;
  // The node which will be succeeding the location being removed from the tree.
  // This is where we start fixing from.
  RbNode* successor;
  // The parent of `successor`.
  RbNode* parent;
  bool deleted_black;
  if (left_ == nullptr) {
    replacer = right_;
    successor = right_;
    parent = parent_;
    deleted_black = this->IsBlack();
    DetachParent(right_);
  } else if (right_ == nullptr) {
    replacer = left_;
    successor = left_;
    parent = parent_;
    deleted_black = this->IsBlack();
    DetachParent(left_);
  } else {
    replacer = left_->RightmostChild();
    successor = replacer->left_;
    parent = replacer->parent_ != this ? replacer->parent_ : replacer;
    deleted_black = replacer->IsBlack();

    // successor does not have a right child. Detach it from its parent and
    // replace it with its left (only) child.
    replacer->DetachParent(successor);

    // Replace this node with the successor.
    replacer->SetLeft(left_);
    replacer->SetRight(right_);
    replacer->SetParentOf(this);
    replacer->red_ = red_;
  }

  // We have already replaced `this` with `successor`. If `this` was previously
  // the root of the tree, we will need to return the updated root.
  std::optional<RbNode*> new_root =
      parent_ == nullptr ? std::optional<RbNode*>(replacer) : std::nullopt;
  if (!deleted_black) {
    return new_root;
  }

  // If `DeleteFix` returns a new root, return that. Otherwise, return
  // `successor` if `this` was previously the root, else root has not changed.
  return OptionalOr(DeleteFix(successor, parent), std::move(new_root));
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
    p = n->Parent();                \
    break;                          \
  } else {                          \
    n->MakeBlack();                 \
    gp->MakeRed();                  \
    p->Rotate##dir##opp(gp, n);     \
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

std::optional<RbNode*> RbNode::DeleteFix(RbNode* n, RbNode* p) {
  while (true) {
#define FIX_CHILD(dir, opp)                           \
  RbNode* s = p->opp();                               \
  CK_ASSERT(s != nullptr);                            \
  if (s->IsRed()) {                                   \
    p->MakeRed();                                     \
    s->MakeBlack();                                   \
    p->Rotate##dir(s);                                \
    s = p->opp();                                     \
    CK_ASSERT(s != nullptr);                          \
  }                                                   \
  if (IsBlackPtr(s->dir()) && IsBlackPtr(s->opp())) { \
    s->MakeRed();                                     \
    n = p;                                            \
    p = n->Parent();                                  \
  } else if (IsRedPtr(s->opp())) {                    \
    s->red_ = p->red_;                                \
    p->MakeBlack();                                   \
    s->opp()->MakeBlack();                            \
    p->Rotate##dir(s);                                \
    n = s;                                            \
    p = n->Parent();                                  \
    break;                                            \
  } else /* IsRedPtr(s->dir()) */ {                   \
    RbNode* sd = s->dir();                            \
    sd->red_ = p->red_;                               \
    p->MakeBlack();                                   \
    s->Rotate##opp##dir(p, sd);                       \
    n = sd;                                           \
    p = n->Parent();                                  \
    break;                                            \
  }

    if (p == nullptr || IsRedPtr(n)) {
      // If we landed on a red node, we can color it black and that will fix
      // the black defecit. If we happened to land on the root, then we need
      // to color it black anyway, so this coincidentally covers both cases.
      if (n != nullptr && n->IsRed()) {
        n->MakeBlack();
      }

      break;
    }

    if (n == p->Left()) {
      FIX_CHILD(Left, Right);
    } else /* n == p->Right() */ {
      FIX_CHILD(Right, Left);
    }

#undef FIX_CHILD
  }

  return p == nullptr ? std::optional<RbNode*>(n)
                      : (p->parent_ == nullptr ? std::optional<RbNode*>(p)
                                               : std::nullopt);
}

}  // namespace ckmalloc
