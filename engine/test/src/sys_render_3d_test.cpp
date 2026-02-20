#include <lithic3d/sys_render_3d.hpp>
#include <gtest/gtest.h>

using namespace lithic3d;

class SysRender3dTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(SysRender3dTest, computeLightProjection_mag_of_light_dir_doesnt_affect_light_space_proj)
{
  float s = 4.f;

  std::array<Vec3f, 8> corners{
    Vec3f{ -s, -s, -s },
    Vec3f{ s, -s, -s },
    Vec3f{ s, -s, s },
    Vec3f{ -s, -s, s },
    Vec3f{ -s, s, -s },
    Vec3f{ s, s, -s },
    Vec3f{ s, s, s },
    Vec3f{ -s, s, s },
  };

  Vec3f lightDirection = Vec3f{ 0, 0, 1 }.normalise();

  auto lightProj1 = computeLightProjection(corners, lightDirection);
  auto lightProj2 = computeLightProjection(corners, lightDirection * 2.f);

  Vec4f p{ 1.f, 1.f, 1.f, 1.f };

  auto p1 = lightProj1.projectionMatrix * lightProj1.viewMatrix * p;
  auto p2 = lightProj2.projectionMatrix * lightProj2.viewMatrix * p;

  EXPECT_EQ(p1[0], p2[0]);
  EXPECT_EQ(p1[1], p2[1]);
  EXPECT_EQ(p1[2], p2[2]);

  EXPECT_TRUE(dbg_isValidFrustum(lightProj1.frustum));
  EXPECT_TRUE(dbg_isValidFrustum(lightProj2.frustum));
}
