#include "src/jsmalloc/collections/intrusive_linked_list.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace jsmalloc {

struct TestItem {
  uint64_t value;

  IntrusiveLinkedList<TestItem>::Node node;
};

TEST(TestIntrusiveLinkedList, SingleElement) {
  IntrusiveLinkedList<TestItem> ll(&TestItem::node);
  TestItem fst{ .value = 1 };
  ll.insert_back(fst);

  EXPECT_EQ(ll.size(), 1);
  EXPECT_EQ(ll.front(), &fst);
  EXPECT_EQ(ll.back(), &fst);
}

TEST(TestIntrusiveLinkedList, Empty) {
  IntrusiveLinkedList<TestItem> ll(&TestItem::node);
  EXPECT_EQ(ll.size(), 0);
  EXPECT_EQ(ll.front(), nullptr);
  EXPECT_EQ(ll.back(), nullptr);
}

TEST(TestIntrusiveLinkedList, Iterates) {
  std::vector<TestItem> vals = {
    TestItem{ .value = 1 },
    TestItem{ .value = 2 },
    TestItem{ .value = 3 },
  };
  IntrusiveLinkedList<TestItem> ll(&TestItem::node);
  for (auto& v : vals) {
    ll.insert_back(v);
  }

  std::vector<uint64_t> got;
  for (auto& v : ll) {
    got.push_back(v.value);
  }
  EXPECT_THAT(got, testing::ElementsAre(1, 2, 3));
}

TEST(TestIntrusiveLinkedList, SupportsDeletion) {
  TestItem vals[] = {
    TestItem{ .value = 1 },
    TestItem{ .value = 2 },
    TestItem{ .value = 3 },
  };
  IntrusiveLinkedList<TestItem> ll(&TestItem::node);
  for (auto& v : vals) {
    ll.insert_back(v);
  }

  ll.remove(vals[1]);

  std::vector<uint64_t> got;
  for (const auto& v : ll) {
    got.push_back(v.value);
  }
  EXPECT_THAT(got, testing::ElementsAre(1, 3));
}

}  // namespace jsmalloc
