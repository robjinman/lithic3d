#include <lithic3d/loose_octree.hpp>
#include <gtest/gtest.h>
#include <vector>

using namespace lithic3d;

class OctreeTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

const Frustum boxFrustum{
  Plane{                            // left
    .normal = { 1.f, 0.f, 0.f },
    .distance = 1.f
  },
  Plane{                            // right
    .normal = { -1.f, 0.f, 0.f },
    .distance = 1.f
  },
  Plane{                            // top
    .normal = { 0.f, -1.f, 0.f },
    .distance = 1.f
  },
  Plane{                            // bottom
    .normal = { 0.f, 1.f, 0.f },
    .distance = 1.f
  },
  Plane{                            // near
    .normal = { 0.f, 0.f, 1.f },
    .distance = 1.f
  },
  Plane{                            // far
    .normal = { 0.f, 0.f, -1.f },
    .distance = 1.f
  }
};

TEST_F(OctreeTest, single_large_item_doesnt_split_root)
{
  LooseOctreePtr octree = createLooseOctree({ -10.f, -10.f, -10.f }, 20.f);

  octree->insert(123, { 1.f, 1.f, 1.f }, 18.f);

  EXPECT_EQ(0, octree->test_root().numChildren);
}

TEST_F(OctreeTest, single_item_causing_single_split)
{
  LooseOctreePtr octree = createLooseOctree({ -10.f, -10.f, -10.f }, 20.f);

  octree->insert(123, { 5.f, 5.f, 5.f }, 4.5f);

  EXPECT_EQ(1, octree->test_root().numChildren);
  EXPECT_NE(nullptr, octree->test_root().children[7]); // Upper-right-far quadrant
}

TEST_F(OctreeTest, single_item_causing_double_split)
{
  LooseOctreePtr octree = createLooseOctree({ -10.f, -10.f, -10.f }, 20.f);

  octree->insert(123, { 2.1f, 2.1f, 2.1f }, 2.f);

  EXPECT_EQ(1, octree->test_root().numChildren);
  ASSERT_NE(nullptr, octree->test_root().children[7]);
  EXPECT_EQ(1, octree->test_root().children[7]->numChildren);
  ASSERT_NE(nullptr, octree->test_root().children[7]->children[0]);
}
