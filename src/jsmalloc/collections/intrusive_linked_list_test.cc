#include "src/jsmalloc/collections/intrusive_linked_list.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/gtest_util.h"

namespace jsmalloc {

struct TestItem : IntrusiveLinkedList<TestItem>::Item {
  uint64_t value;
};

TEST(TestIntrusiveLinkedList, SingleElement) {
  IntrusiveLinkedList<TestItem> ll;
  TestItem fst{ .value = 1 };
  ll.insert_back(fst);

  EXPECT_EQ(ll.size(), 1);
  EXPECT_EQ(ll.front(), &fst);
  EXPECT_EQ(ll.back(), &fst);
}

TEST(TestIntrusiveLinkedList, Empty) {
  IntrusiveLinkedList<TestItem> ll;
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
  IntrusiveLinkedList<TestItem> ll;
  for (auto& v : vals) {
    ll.insert_back(v);
  }

  std::vector<uint64_t> got;
  for (auto& v : ll) {
    got.push_back(v.value);
  }

  EXPECT_THAT(got, testing::ElementsAre(1, 2, 3));
}

}  // namespace jsmalloc
