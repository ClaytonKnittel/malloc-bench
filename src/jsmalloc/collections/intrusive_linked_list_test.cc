#include "src/jsmalloc/collections/intrusive_linked_list.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

struct TestItem {
  uint64_t value;

  class List : public IntrusiveLinkedList<TestItem, List> {
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
  ll.insert_back(fst);

  EXPECT_FALSE(ll.empty());
  EXPECT_EQ(ll.front(), &fst);
  EXPECT_EQ(ll.back(), &fst);
}

TEST(TestIntrusiveLinkedList, Empty) {
  TestItem::List ll;
  EXPECT_TRUE(ll.empty());
  EXPECT_EQ(ll.front(), nullptr);
  EXPECT_EQ(ll.back(), nullptr);
}

TEST(TestIntrusiveLinkedList, Iterates) {
  std::vector<TestItem> vals = {
    TestItem{ .value = 1 },
    TestItem{ .value = 2 },
    TestItem{ .value = 3 },
  };
  TestItem::List ll;
  for (auto& v : vals) {
    ll.insert_back(v);
  }

  std::vector<uint64_t> got;
  for (auto& v : ll) {
    got.push_back(v.value);
  };
  EXPECT_THAT(got, testing::ElementsAre(1, 2, 3));
}

TEST(TestIntrusiveLinkedList, SupportsDeletion) {
  TestItem vals[] = {
    TestItem{ .value = 1 },
    TestItem{ .value = 2 },
    TestItem{ .value = 3 },
  };
  TestItem::List ll;
  for (auto& v : vals) {
    ll.insert_back(v);
  }

  TestItem::List::unlink(vals[1]);

  std::vector<uint64_t> got;
  for (const auto& v : ll) {
    got.push_back(v.value);
  }
  EXPECT_THAT(got, testing::ElementsAre(1, 3));
}

}  // namespace jsmalloc
