#include "src/ckmalloc/red_black_tree.h"

#include "src/ckmalloc/util.h"

namespace ckmalloc {

std::optional<RbNode*> RbNode::InsertLeft(RbNode* node) {
  CK_ASSERT(node->left_ == nullptr);
  node->left_ = this;
  this->parent_ = node;
  this->red_ = true;
  return InsertFix(this);
}

std::optional<RbNode*> RbNode::InsertRight(RbNode* node) {
  CK_ASSERT(node->right_ == nullptr);
  node->right_ = this;
  this->parent_ = node;
  this->red_ = true;
  return InsertFix(this);
}

RbNode* RbNode::Remove() {
  return nullptr;
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

void RbNode::SetParentOf(RbNode* node) {
  parent_ = node->parent_;
  if (parent_ != nullptr) {
    if (parent_->left_ == node) {
      parent_->left_ = this;
    } else {
      parent_->right_ = this;
    }
  }
}

/* static */
std::optional<RbNode*> RbNode::InsertFix(RbNode* n) {
  RbNode* p;
  while ((p = n->parent_) != nullptr && p->red_) {
    RbNode* gp = p->parent_;
    if (p == gp->left_) {
      RbNode* a = gp->right_;
      if (a != nullptr && a->red_) {
        p->MakeBlack();
        a->MakeBlack();
        gp->MakeRed();
        n = gp;
      } else if (n == p->left_) {
        p->MakeBlack();
        gp->MakeRed();
        gp->SetLeft(p->right_);
        p->SetParentOf(gp);
        p->SetRight(gp);
        n = p;
      } else {
        n->MakeBlack();
        gp->MakeRed();
        p->SetRight(n->left_);
        gp->SetLeft(n->right_);
        n->SetParentOf(gp);
        n->SetLeft(p);
        n->SetRight(gp);
      }
    } else /* p == gp->right_ */ {
      RbNode* a = gp->left_;
      if (a != nullptr && a->red_) {
        p->MakeBlack();
        a->MakeBlack();
        gp->MakeRed();
        n = gp;
      } else if (n == p->right_) {
        p->MakeBlack();
        gp->MakeRed();
        gp->SetRight(p->left_);
        p->SetParentOf(gp);
        p->SetLeft(gp);
        n = p;
      } else {
        n->MakeBlack();
        gp->MakeRed();
        p->SetLeft(n->right_);
        gp->SetRight(n->left_);
        n->SetParentOf(gp);
        n->SetRight(p);
        n->SetLeft(gp);
      }
    }
  }

  if (p != nullptr) {
    return std::nullopt;
  }

  n->MakeBlack();
  return n;
}

}  // namespace ckmalloc
