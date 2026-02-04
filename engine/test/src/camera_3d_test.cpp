#include <lithic3d/camera_3d.hpp>
#include <lithic3d/units.hpp>
#include <gtest/gtest.h>
#include <vector>

using namespace lithic3d;

class Camera3dTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(Camera3dTest, computeFrustum_simple_square_frustum)
{
  float aspectRatio = 1.f;

  Camera3d camera{1.f, 0.f};

  float vFov = 45.f;
  float hFov = 2.f * atan(aspectRatio * tan(0.5f * vFov));
  float nearPlane = metresToWorldUnits(0.1f);
  float farPlane = metresToWorldUnits(1000.f);

  const float epsilon = 0.0001f;

  auto frustum = camera.computeFrustum();
  EXPECT_NEAR(nearPlane, frustum[FrustumPlane::Near].distance, epsilon);
}
