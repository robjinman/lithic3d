#include <lithic3d/renderables.hpp>
#include <gtest/gtest.h>

using namespace lithic3d;
using namespace lithic3d::render;

class AlignedBytesTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(AlignedBytesTest, stores_single_element_from_vector)
{
  std::vector<Vec3f> data{
    Vec3f{ 1, 2, 3 }
  };
  AlignedBytes buffer{data};

  ASSERT_EQ(1, buffer.numElements());
  EXPECT_EQ(1 * sizeof(Vec3f), buffer.sizeInBytes());

  Vec3f expected{ 1, 2, 3 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(0));
}

TEST_F(AlignedBytesTest, stores_multiple_elements_from_vector)
{
  std::vector<Vec3f> data{
    Vec3f{ 1, 2, 3 },
    Vec3f{ 4, 5, 6 }
  };
  AlignedBytes buffer{data};

  ASSERT_EQ(2, buffer.numElements());
  EXPECT_EQ(2 * sizeof(Vec3f), buffer.sizeInBytes());

  Vec3f expected{ 1, 2, 3 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(0));

  expected = { 4, 5, 6 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(1));
}

TEST_F(AlignedBytesTest, construction_from_size_and_initial_value)
{
  AlignedBytes buffer{2, Vec3f{ 1, 2, 3 }};

  ASSERT_EQ(2, buffer.numElements());
  EXPECT_EQ(2 * sizeof(Vec3f), buffer.sizeInBytes());

  Vec3f expected{ 1, 2, 3 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(0));
  EXPECT_EQ(expected, buffer.element<Vec3f>(1));
}

TEST_F(AlignedBytesTest, move_constructor)
{
  std::vector<Vec3f> data{
    Vec3f{ 1, 2, 3 },
    Vec3f{ 4, 5, 6 }
  };
  AlignedBytes original{data};

  AlignedBytes buffer{std::move(original)};

  ASSERT_EQ(2, buffer.numElements());
  EXPECT_EQ(2 * sizeof(Vec3f), buffer.sizeInBytes());

  Vec3f expected{ 1, 2, 3 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(0));

  expected = { 4, 5, 6 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(1));
}

TEST_F(AlignedBytesTest, move_assignment)
{
  std::vector<Vec3f> data{
    Vec3f{ 1, 2, 3 },
    Vec3f{ 4, 5, 6 }
  };
  AlignedBytes original{data};

  AlignedBytes buffer = std::move(original);

  ASSERT_EQ(2, buffer.numElements());
  EXPECT_EQ(2 * sizeof(Vec3f), buffer.sizeInBytes());

  Vec3f expected{ 1, 2, 3 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(0));

  expected = { 4, 5, 6 };
  EXPECT_EQ(expected, buffer.element<Vec3f>(1));
}
