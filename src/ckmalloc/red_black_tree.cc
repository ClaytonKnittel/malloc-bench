#include "src/ckmalloc/red_black_tree.h"

#include <iomanip>
#include <optional>

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

std::optional<RbNode*> RbNode::Remove() {
  RbNode* successor;
  if (left_ == nullptr) {
    if (right_ == nullptr) {
      successor = nullptr;
    } else {
      successor = right_->LeftmostChild();
      // successor does not have a left child. Detach it from its parent and
      // replace it with its right (only) child.
      successor->DetachParent(successor->right_);
    }
  } else {
    successor = left_->RightmostChild();
    // successor does not have a right child. Detach it from its parent and
    // replace it with its left (only) child.
    successor->DetachParent(successor->left_);
  }

  // Even though we have detached successor from the tree, it's parent_ pointer
  // is still intact. Determine if we need to fix the tree, and if so where to
  // start from.
  //
  // No need to fix the tree if we are removing a red node. Otherwise, fix
  // starting from successor's former parent. If there was no successor, fix
  // from our parent if this node is black, otherwise no need to fix.
  RbNode* correct_from = successor != nullptr ? successor : this;
  RbNode* parent_to_fix =
      correct_from->IsRed() ? nullptr : correct_from->parent_;

  // Replace this node with its successor.
  if (successor != nullptr) {
    successor->SetLeft(left_);
    successor->SetRight(right_);
    successor->SetParentOf(this);
    successor->red_ = red_;
  } else {
    DetachParent(nullptr);
  }

  if (parent_to_fix == nullptr) {
    if (parent_ == nullptr) {
      // If we removed the root of the tree, return the new root.
      return successor;
    }

    // Otherwise, since the root was not changed, return `nullopt`.
    return std::nullopt;
  }

  return DeleteFix(parent_to_fix);
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

void RbNode::DetachParent(RbNode* new_child) {
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
        p = n->parent_;
        break;
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
        p = n->parent_;
        break;
      }
    }
  }

  if (p != nullptr) {
    return std::nullopt;
  }

  n->MakeBlack();
  return n;
}

std::optional<RbNode*> RbNode::DeleteFix(RbNode* n) {}

}  // namespace ckmalloc
