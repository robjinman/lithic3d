#include <lithic3d/graph.hpp>
#include <gtest/gtest.h>
#include <vector>

using namespace lithic3d;

class GraphTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(GraphTest, single_child_stored_after_parent)
{
  Graph<int, int, -1> graph(0, 123);

  graph.addItem(1, 0, 234);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(1, graph.dfs[1].key);
  EXPECT_EQ(234, graph.dfs[1].value);
}

TEST_F(GraphTest, two_children_stored_after_parent)
{
  Graph<int, int, -1> graph(0, 123);

  graph.addItem(1, 0, 234);
  graph.addItem(2, 0, 345);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(1, graph.dfs[1].key);
  EXPECT_EQ(234, graph.dfs[1].value);
  EXPECT_EQ(2, graph.dfs[2].key);
  EXPECT_EQ(345, graph.dfs[2].value);
}

TEST_F(GraphTest, child_added_to_first_child_stored_after_first_child)
{
  Graph<int, int, -1> graph(0, 123);

  graph.addItem(1, 0, 234);
  graph.addItem(2, 0, 345);
  graph.addItem(3, 1, 456);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(1, graph.dfs[1].key);
  EXPECT_EQ(234, graph.dfs[1].value);
  EXPECT_EQ(3, graph.dfs[2].key);
  EXPECT_EQ(456, graph.dfs[2].value);
  EXPECT_EQ(2, graph.dfs[3].key);
  EXPECT_EQ(345, graph.dfs[3].value);
}

TEST_F(GraphTest, remove_item_with_no_children)
{
  Graph<int, int, -1> graph(0, 123);

  graph.addItem(1, 0, 234);
  graph.addItem(2, 0, 345);
  graph.addItem(3, 1, 456);

  graph.removeItem(3);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(1, graph.dfs[1].key);
  EXPECT_EQ(234, graph.dfs[1].value);
  EXPECT_EQ(2, graph.dfs[2].key);
  EXPECT_EQ(345, graph.dfs[2].value);
}

TEST_F(GraphTest, child_stored_after_grand_nephew)
{
  Graph<int, int, -1> graph(0, 123);

  // 0
  // └─ 1
  //    ├─ 2
  //    │  └─ 3
  //    │     └─ 4
  //    └─ 5

  graph.addItem(1, 0, 234);
  graph.addItem(2, 1, 345);
  graph.addItem(3, 2, 456);
  graph.addItem(4, 3, 567);
  graph.addItem(5, 1, 678);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(1, graph.dfs[1].key);
  EXPECT_EQ(234, graph.dfs[1].value);
  EXPECT_EQ(2, graph.dfs[2].key);
  EXPECT_EQ(345, graph.dfs[2].value);
  EXPECT_EQ(3, graph.dfs[3].key);
  EXPECT_EQ(456, graph.dfs[3].value);
  EXPECT_EQ(4, graph.dfs[4].key);
  EXPECT_EQ(567, graph.dfs[4].value);
  EXPECT_EQ(5, graph.dfs[5].key);
  EXPECT_EQ(678, graph.dfs[5].value);
}

TEST_F(GraphTest, remove_item_with_child)
{
  Graph<int, int, -1> graph(0, 123);

  graph.addItem(1, 0, 234);
  graph.addItem(2, 0, 345);
  graph.addItem(3, 1, 456);

  graph.removeItem(1);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(2, graph.dfs[1].key);
  EXPECT_EQ(345, graph.dfs[1].value);
}

TEST_F(GraphTest, remove_item_with_children)
{
  Graph<int, int, -1> graph(0, 123);

  graph.addItem(1, 0, 234);
  graph.addItem(2, 0, 345);
  graph.addItem(3, 1, 456);
  graph.addItem(4, 1, 567);
  graph.addItem(5, 0, 678);

  graph.removeItem(1);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(2, graph.dfs[1].key);
  EXPECT_EQ(345, graph.dfs[1].value);
  EXPECT_EQ(5, graph.dfs[2].key);
  EXPECT_EQ(678, graph.dfs[2].value);
}

TEST_F(GraphTest, remove_item_with_grandchildren)
{
  Graph<int, int, -1> graph(0, 123);

  graph.addItem(1, 0, 234);
  graph.addItem(2, 0, 345);
  graph.addItem(3, 1, 456);
  graph.addItem(4, 1, 567);
  graph.addItem(5, 0, 678);
  graph.addItem(6, 3, 789);
  graph.addItem(7, 3, 890);
  graph.addItem(8, 4, 901);

  graph.removeItem(1);

  EXPECT_EQ(0, graph.dfs[0].key);
  EXPECT_EQ(123, graph.dfs[0].value);
  EXPECT_EQ(2, graph.dfs[1].key);
  EXPECT_EQ(345, graph.dfs[1].value);
  EXPECT_EQ(5, graph.dfs[2].key);
  EXPECT_EQ(678, graph.dfs[2].value);
}
