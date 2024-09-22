#include "src/jsmalloc/collections/intrusive_stack.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

struct TestItem {
  uint64_t value;

  class List : public IntrusiveStack<TestItem, List> {
   public:
    constexpr static Node* GetNode(TestItem* item) {
      return &item->node_;
    }
    constexpr static TestItem* GetItem(Node* node) {
      return twiddle::OwnerOf(node, &TestItem::node_);
    }
  };

  List::Node node_;
};

TEST(TestIntrusiveLinkedList, SingleElement) {
  TestItem fst{ .value = 1 };

  TestItem::List ll;
  ll.push(fst);

  EXPECT_FALSE(ll.empty());
  EXPECT_EQ(ll.peek(), &fst);
  ll.pop();
  EXPECT_TRUE(ll.empty());
}

TEST(TestIntrusiveLinkedList, MultipleElements) {
  std::vector<TestItem> vals = {
    TestItem{ .value = 1 },
    TestItem{ .value = 2 },
    TestItem{ .value = 3 },
  };

  TestItem::List ll;
  for (auto& v : vals) {
    ll.push(v);
  }

  std::vector<uint64_t> got;
  while (!ll.empty()) {
    got.push_back(ll.pop()->value);
  };
  EXPECT_THAT(got, testing::ElementsAre(3, 2, 1));
}

}  // namespace jsmalloc
