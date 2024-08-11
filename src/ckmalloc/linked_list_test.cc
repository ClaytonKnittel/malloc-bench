#include "src/ckmalloc/linked_list.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ckmalloc {

using testing::ElementsAre;
using testing::Field;

class LinkedListTest : public ::testing::Test {};

struct Item : public LinkedListNode {
  uint64_t val;
};

TEST_F(LinkedListTest, TestEmpty) {
  LinkedList<Item> list;
  EXPECT_TRUE(list.Empty());
  EXPECT_EQ(list.Front(), nullptr);
  EXPECT_EQ(list.Back(), nullptr);
}

TEST_F(LinkedListTest, OneItem) {
  LinkedList<Item> list;
  Item item = { .val = 10 };
  list.InsertFront(&item);
  EXPECT_FALSE(list.Empty());
  EXPECT_EQ(list.Front(), &item);
  EXPECT_EQ(list.Back(), &item);
}

TEST_F(LinkedListTest, InsertFront) {
  LinkedList<Item> list;
  Item items[] = {
    { .val = 10 },
    { .val = 20 },
    { .val = 30 },
  };
  list.InsertFront(&items[0]);
  list.InsertFront(&items[1]);
  list.InsertFront(&items[2]);
  EXPECT_FALSE(list.Empty());
  EXPECT_EQ(list.Front(), &items[2]);
  EXPECT_EQ(list.Back(), &items[0]);

  EXPECT_THAT(list, ElementsAre(Field(&Item ::val, 30), Field(&Item ::val, 20),
                                Field(&Item ::val, 10)));
}

TEST_F(LinkedListTest, InsertBack) {
  LinkedList<Item> list;
  Item items[] = {
    { .val = 10 },
    { .val = 20 },
    { .val = 30 },
  };
  list.InsertBack(&items[0]);
  list.InsertBack(&items[1]);
  list.InsertBack(&items[2]);
  EXPECT_FALSE(list.Empty());
  EXPECT_EQ(list.Front(), &items[0]);
  EXPECT_EQ(list.Back(), &items[2]);

  EXPECT_THAT(list, ElementsAre(Field(&Item ::val, 10), Field(&Item ::val, 20),
                                Field(&Item ::val, 30)));
}

TEST_F(LinkedListTest, InsertAfter) {
  LinkedList<Item> list;
  Item items[] = {
    { .val = 10 },
    { .val = 20 },
    { .val = 30 },
    { .val = 40 },
  };
  list.InsertBack(&items[0]);
  list.InsertBack(&items[1]);
  list.InsertBack(&items[2]);

  auto it = list.begin();
  ++it;
  list.InsertAfter(it, &items[3]);

  EXPECT_THAT(list, ElementsAre(Field(&Item ::val, 10), Field(&Item ::val, 20),
                                Field(&Item::val, 40), Field(&Item ::val, 30)));
}

TEST_F(LinkedListTest, Remove) {
  LinkedList<Item> list;
  Item items[] = {
    { .val = 10 }, { .val = 20 }, { .val = 30 }, { .val = 40 }, { .val = 50 },
  };
  list.InsertBack(&items[0]);
  list.InsertBack(&items[1]);
  list.InsertBack(&items[2]);
  list.InsertBack(&items[3]);
  list.InsertBack(&items[4]);

  list.Remove(list.begin());
  list.Remove(++list.begin());

  EXPECT_THAT(list, ElementsAre(Field(&Item ::val, 20), Field(&Item ::val, 40),
                                Field(&Item ::val, 50)));
}

}  // namespace ckmalloc
