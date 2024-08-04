#include "src/ckmalloc/red_black_tree.h"

#include <iomanip>
#include <ostream>
#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

namespace ckmalloc {

using ::testing::Field;
using ::testing::Not;
using ::testing::Pointee;
using util::IsOk;

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
  static std::string PrintNode(const RbNode* node, int depth) {
    if (node == nullptr) {
      return "";
    }

    std::ostringstream ostr;
    ostr << std::setw(2 * depth) << "" << *static_cast<const T*>(node)
         << std::endl;
    ostr << PrintNode<T>(node->Left(), depth + 1);
    ostr << PrintNode<T>(node->Right(), depth + 1);
    return ostr.str();
  }

  template <typename T, typename Cmp>
  static std::string Print(const RbTree<T, Cmp>& tree) {
    return PrintNode<T>(tree.Root(), 0);
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
  EXPECT_THAT(Validate(tree), IsOk());
}

TEST_F(RedBlackTreeTest, TestSingle) {
  ElementTree tree;
  Element root = { .val = 1 };
  tree.Insert(&root);

  EXPECT_EQ(tree.LowerBound([](const Element&) { return true; }), &root);

  EXPECT_EQ(
      tree.LowerBound([](const Element& element) { return element.val > 1; }),
      nullptr);
  EXPECT_THAT(Validate(tree), IsOk());
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
  EXPECT_THAT(Validate(tree), IsOk());
}

TEST_F(RedBlackTreeTest, TestInsertMany) {
  constexpr size_t kNumElements = 1000;

  ElementTree tree;
  Element elements[kNumElements];
  for (size_t i = 0; i < kNumElements; i++) {
    elements[i].val = (i * 13) % kNumElements;
    tree.Insert(&elements[i]);
    ASSERT_THAT(Validate(tree), IsOk());
    ASSERT_EQ(tree.Size(), i + 1);
  }

  for (size_t i = 0; i < kNumElements; i++) {
    Element* element = tree.LowerBound(
        [i](const Element& element) { return element.val >= i; });
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(((element - &elements[0]) * 13) % kNumElements, i);
  }
}

TEST_F(RedBlackTreeTest, TestDeleteMany) {
  constexpr size_t kNumElements = 20;
  return;

  ElementTree tree;
  Element elements[kNumElements];
  for (size_t i = 0; i < kNumElements; i++) {
    elements[i].val = (i * 17) % kNumElements;
    tree.Insert(&elements[i]);
  }

  ASSERT_THAT(Validate(tree), IsOk());

  for (size_t i = 0; i < kNumElements; i++) {
    size_t idx = (i * 19 + 3) % kNumElements;
    tree.Remove(&elements[idx]);
    ASSERT_THAT(Validate(tree), IsOk());
    ASSERT_EQ(tree.Size(), kNumElements - i - 1);

    for (size_t j = 0; j < kNumElements; j++) {
      Element* find_j = tree.LowerBound(
          [j](const Element& element) { return element.val >= j; });
      auto is_ptr_to_j = Pointee(Field(&Element::val, j));
      if (j <= i) {
        ASSERT_THAT(find_j, Not(is_ptr_to_j));
      } else {
        ASSERT_THAT(find_j, is_ptr_to_j);
      }
    }
  }
}

}  // namespace ckmalloc
