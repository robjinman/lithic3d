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

TEST_F(GraphTest, addItem)
{
  Graph<int, -1> graph(0);

  graph.addItem(1, 0);

  // TODO
}
