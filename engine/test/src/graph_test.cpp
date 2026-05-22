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
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(1, graph.dfs[1]);
}

TEST_F(GraphTest, two_children_stored_after_parent)
{
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);
  graph.addItem(2, 0);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(1, graph.dfs[1]);
  EXPECT_EQ(2, graph.dfs[2]);
}

TEST_F(GraphTest, grandchild_of_first_child_stored_after_first_child)
{
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);
  graph.addItem(2, 0);
  graph.addItem(3, 1);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(1, graph.dfs[1]);
  EXPECT_EQ(3, graph.dfs[2]);
  EXPECT_EQ(2, graph.dfs[3]);
}

TEST_F(GraphTest, remove_item_with_no_children)
{
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);
  graph.addItem(2, 0);
  graph.addItem(3, 1);

  graph.removeItem(3);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(1, graph.dfs[1]);
  EXPECT_EQ(2, graph.dfs[2]);
}

TEST_F(GraphTest, child_stored_after_grand_nephew)
{
  Graph<int, -1> graph(0);

  // 0
  // └─ 1
  //    ├─ 2
  //    │  └─ 3
  //    │     └─ 4
  //    └─ 5

  graph.addItem(1, 0);
  graph.addItem(2, 1);
  graph.addItem(3, 2);
  graph.addItem(4, 3);
  graph.addItem(5, 1);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(1, graph.dfs[1]);
  EXPECT_EQ(2, graph.dfs[2]);
  EXPECT_EQ(3, graph.dfs[3]);
  EXPECT_EQ(4, graph.dfs[4]);
  EXPECT_EQ(5, graph.dfs[5]);
}

TEST_F(GraphTest, remove_item_with_child)
{
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);
  graph.addItem(2, 0);
  graph.addItem(3, 1);

  graph.removeItem(1);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(2, graph.dfs[1]);
}

TEST_F(GraphTest, remove_item_with_children)
{
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);
  graph.addItem(2, 0);
  graph.addItem(3, 1);
  graph.addItem(4, 1);
  graph.addItem(5, 0);

  graph.removeItem(1);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(2, graph.dfs[1]);
  EXPECT_EQ(5, graph.dfs[2]);
}

TEST_F(GraphTest, remove_item_with_grandchildren)
{
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);
  graph.addItem(2, 0);
  graph.addItem(3, 1);
  graph.addItem(4, 1);
  graph.addItem(5, 0);
  graph.addItem(6, 3);
  graph.addItem(7, 3);
  graph.addItem(8, 4);

  graph.removeItem(1);

  EXPECT_EQ(0, graph.dfs[0]);
  EXPECT_EQ(2, graph.dfs[1]);
  EXPECT_EQ(5, graph.dfs[2]);
}
