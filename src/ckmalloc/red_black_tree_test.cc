#include "src/ckmalloc/red_black_tree.h"

#include <iomanip>
#include <ostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

namespace ckmalloc {

class RedBlackTreeTest : public ::testing::Test {
 protected:
  template <typename T, typename Cmp>
  static absl::Status Validate(const RbTree<T, Cmp>& tree) {
    if (tree.Root() != nullptr) {
      if (tree.Root()->Parent() != nullptr) {
        return absl::FailedPreconditionError(
            "Found root with non-null parent.");
      }
    }
    return ValidateNode<T, Cmp>(tree.Root()).status();
  }

  template <typename T>
  static void PrintNode(const RbNode* node, int depth) {
    if (node == nullptr) {
      return;
    }

    std::cout << std::setw(2 * depth) << "" << *static_cast<const T*>(node)
              << std::endl;
    PrintNode<T>(node->Left(), depth + 1);
    PrintNode<T>(node->Right(), depth + 1);
  }

  template <typename T, typename Cmp>
  static void Print(const RbTree<T, Cmp>& tree) {
    PrintNode<T>(tree.Root(), 0);
  }

 private:
  // If valid, returns the black depth of the node.
  template <typename T, typename Cmp>
  static absl::StatusOr<size_t> ValidateNode(const RbNode* node) {
    if (node == nullptr) {
      return 0;
    }

    if (node->Left() != nullptr) {
      if (node->Left()->Parent() != node) {
        return absl::FailedPreconditionError(
            "Found left child with parent incorrect");
      }
      if (!Cmp{}(*static_cast<const T*>(node->Left()),
                 *static_cast<const T*>(node))) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Found left child of node >= node"));
      }
      if (node->IsRed() && node->Left()->IsRed()) {
        return absl::FailedPreconditionError(
            "Found left child of red node which is also red.");
      }
    }
    if (node->Right() != nullptr) {
      if (node->Right()->Parent() != node) {
        return absl::FailedPreconditionError(
            "Found right child with parent incorrect");
      }
      if (!Cmp{}(*static_cast<const T*>(node),
                 *static_cast<const T*>(node->Right()))) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Found right child of node < node"));
      }
      if (node->IsRed() && node->Right()->IsRed()) {
        return absl::FailedPreconditionError(
            "Found right child of red node which is also red.");
      }
    }

    DEFINE_OR_RETURN(size_t, left_depth, ValidateNode<T, Cmp>(node->Left()));
    DEFINE_OR_RETURN(size_t, right_depth, ValidateNode<T, Cmp>(node->Right()));

    if (left_depth != right_depth) {
      std::cout << "Diff depths: " << *static_cast<const T*>(node->Left())
                << " vs " << *static_cast<const T*>(node->Right()) << std::endl;
      return absl::FailedPreconditionError(
          absl::StrFormat("Found inequal black depth of node: %zu vs %zu",
                          left_depth, right_depth));
    }

    return left_depth + (node->IsRed() ? 0 : 1);
  }
};

struct Element : public RbNode {
  int val;
};

std::ostream& operator<<(std::ostream& ostr, const Element& element) {
  return ostr << element.val << (element.IsRed() ? " (r)" : " (b)");
}

struct ElementLess {
  bool operator()(const Element& e1, const Element& e2) const {
    return e1.val < e2.val;
  }
};

using ElementTree = RbTree<Element, ElementLess>;

TEST_F(RedBlackTreeTest, TestEmpty) {
  ElementTree tree;
  EXPECT_EQ(tree.LowerBound([](const Element&) { return true; }), nullptr);
  EXPECT_THAT(Validate(tree), util::IsOk());
}

TEST_F(RedBlackTreeTest, TestSingle) {
  ElementTree tree;
  Element root = { .val = 1 };
  tree.Insert(&root);

  EXPECT_EQ(tree.LowerBound([](const Element&) { return true; }), &root);

  EXPECT_EQ(
      tree.LowerBound([](const Element& element) { return element.val > 1; }),
      nullptr);
  EXPECT_THAT(Validate(tree), util::IsOk());
}

TEST_F(RedBlackTreeTest, TestTwo) {
  ElementTree tree;
  Element root = { .val = 1 };
  Element child = { .val = 2 };
  tree.Insert(&root);
  tree.Insert(&child);

  EXPECT_EQ(
      tree.LowerBound([](const Element& element) { return element.val > 0; }),
      &root);

  EXPECT_EQ(
      tree.LowerBound([](const Element& element) { return element.val > 1; }),
      &child);
  EXPECT_THAT(Validate(tree), util::IsOk());
}

TEST_F(RedBlackTreeTest, TestMany) {
  constexpr size_t kNumElements = 1000;

  ElementTree tree;
  Element elements[kNumElements];
  for (size_t i = 0; i < kNumElements; i++) {
    elements[i].val = (i + 17) % kNumElements;
    tree.Insert(&elements[i]);
    ASSERT_THAT(Validate(tree), util::IsOk());
  }

  for (size_t i = 0; i < kNumElements; i++) {
    Element* element = tree.LowerBound(
        [i](const Element& element) { return element.val >= i; });
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(((element - &elements[0]) + 17) % kNumElements, i);
  }
}

}  // namespace ckmalloc
