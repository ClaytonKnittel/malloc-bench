#include "src/ckmalloc/red_black_tree.h"

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
  CK_ASSERT_EQ(right, right_);
  this->SetRight(right->Left());
  right->SetParentOf(this);
  this->parent_ = right;
  right->left_ = this;
}

void RbNode::RotateRight(RbNode* left) {
  CK_ASSERT_EQ(left, left_);
  this->SetLeft(left->Right());
  left->SetParentOf(this);
  this->parent_ = left;
  left->right_ = this;
}

void RbNode::RotateLeftRight(RbNode* parent, RbNode* right) {
  CK_ASSERT_EQ(parent, parent_);
  CK_ASSERT_EQ(parent->left_, this);
  CK_ASSERT_EQ(right, right_);
  this->SetRight(right->Left());
  parent->SetLeft(right->Right());
  right->SetParentOf(parent);
  right->SetLeft(this);
  right->SetRight(parent);
}

void RbNode::RotateRightLeft(RbNode* parent, RbNode* left) {
  CK_ASSERT_EQ(parent, parent_);
  CK_ASSERT_EQ(parent->right_, this);
  CK_ASSERT_EQ(left, left_);
  this->SetLeft(left->Right());
  parent->SetRight(left->Left());
  left->SetParentOf(parent);
  left->SetRight(this);
  left->SetLeft(parent);
}

void RbNode::InsertLeft(RbNode* node, const RbNode* root) {
  CK_ASSERT_EQ(node->left_, nullptr);
  node->left_ = this;
  this->parent_ = node;
  this->MakeRed();
  InsertFix(this, root);
}

void RbNode::InsertRight(RbNode* node, const RbNode* root) {
  CK_ASSERT_EQ(node->right_, nullptr);
  node->right_ = this;
  this->parent_ = node;
  this->MakeRed();
  InsertFix(this, root);
}

void RbNode::Remove(const RbNode* root) const {
  // The node which will be succeeding the location being removed from the tree.
  // This is where we start fixing from.
  RbNode* successor;
  // The parent of `successor`.
  RbNode* parent;
  bool deleted_black;
  if (left_ == nullptr) {
    successor = right_;
    parent = parent_;
    deleted_black = this->IsBlack();
    DetachParent(right_);
  } else if (right_ == nullptr) {
    successor = left_;
    parent = parent_;
    deleted_black = this->IsBlack();
    DetachParent(left_);
  } else {
    RbNode* scapegoat = left_->RightmostChild();
    successor = scapegoat->left_;
    parent = scapegoat->parent_ != this ? scapegoat->parent_ : scapegoat;
    deleted_black = scapegoat->IsBlack();

    // successor does not have a right child. Detach it from its parent and
    // replace it with its left (only) child.
    scapegoat->DetachParent(successor);

    // Replace this node with the successor.
    scapegoat->SetLeft(left_);
    scapegoat->SetRight(right_);
    scapegoat->SetParentOf(this);
    scapegoat->red_ = red_;
  }

  if (deleted_black) {
    DeleteFix(successor, parent, root);
  }
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

void RbNode::InsertFix(RbNode* n, const RbNode* root) {
  RbNode* p;
  while ((p = n->parent_) != root && p->IsRed()) {
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

  if (p == root) {
    n->MakeBlack();
  }
}

void RbNode::DeleteFix(RbNode* n, RbNode* p, const RbNode* root) {
  while (true) {
#define FIX_CHILD(dir, opp)                           \
  RbNode* s = p->opp();                               \
  CK_ASSERT_NE(s, nullptr);                           \
  if (s->IsRed()) {                                   \
    p->MakeRed();                                     \
    s->MakeBlack();                                   \
    p->Rotate##dir(s);                                \
    s = p->opp();                                     \
    CK_ASSERT_NE(s, nullptr);                         \
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

    if (p == root || IsRedPtr(n)) {
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
}

}  // namespace ckmalloc
