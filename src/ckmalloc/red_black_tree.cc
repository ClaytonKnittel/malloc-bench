#include "src/ckmalloc/red_black_tree.h"

#include <iomanip>

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
  return std::nullopt;
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

}  // namespace ckmalloc
