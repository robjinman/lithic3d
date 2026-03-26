#include <lithic3d/sys_collision.hpp>
#include <gtest/gtest.h>

using namespace lithic3d;

class SysCollisionTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(SysCollisionTest, HeightMapSampler_smallest_map)
{
  HeightMap heightMap{
    .width = 1.f,
    .height = 1.f,
    .widthPx = 2,
    .heightPx = 2,
    .data{
      1.f, 2.f, 3.f, 4.f
    }
  };
  HeightMapSampler sampler{heightMap, {}};

  auto t = sampler.triangle({ 0.25f, 0.25f });

  Vec3f A{ 0.f, 3.f, 1.f };
  Vec3f B{ 1.f, 4.f, 1.f };
  Vec3f C{ 1.f, 2.f, 0.f };
  Vec3f D{ 0.f, 1.f, 0.f };

  EXPECT_EQ(A, t[0]);
  EXPECT_EQ(C, t[1]);
  EXPECT_EQ(D, t[2]);

  t = sampler.triangle({ 0.75f, 0.75f });

  EXPECT_EQ(A, t[0]);
  EXPECT_EQ(B, t[1]);
  EXPECT_EQ(C, t[2]);
}
