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

TEST_F(Camera3dTest, computeFrustum_camera_at_origin_square_aspect)
{
  float aspectRatio = 1.f;
  Camera3d camera{aspectRatio, 0.f};

  float vFov = degreesToRadians(45.f);
  float hFov = 2.f * atan(aspectRatio * tan(0.5f * vFov));
  float nearPlane = metresToWorldUnits(0.1f);
  float farPlane = metresToWorldUnits(1000.f);

  // Accuracy is very low for some reason
  const float epsilon = 1.f;

  auto frustum = camera.getWorldSpaceFrustum();
  std::cout << "Near: " << frustum.at(FrustumPlane::Near).normal << " " << frustum.at(FrustumPlane::Near).distance << "\n";
  std::cout << "Far: " << frustum.at(FrustumPlane::Far).normal << " " << frustum.at(FrustumPlane::Far).distance << "\n";
  std::cout << "Left: " << frustum.at(FrustumPlane::Left).normal << " " << frustum.at(FrustumPlane::Left).distance << "\n";
  std::cout << "Right: " << frustum.at(FrustumPlane::Right).normal << " " << frustum.at(FrustumPlane::Right).distance << "\n";
  std::cout << "bottom: " << frustum.at(FrustumPlane::Bottom).normal << " " << frustum.at(FrustumPlane::Bottom).distance << "\n";
  std::cout << "Top: " << frustum.at(FrustumPlane::Top).normal << " " << frustum.at(FrustumPlane::Top).distance << "\n";

  EXPECT_NEAR(nearPlane, frustum[FrustumPlane::Near].distance, epsilon);
  EXPECT_NEAR(farPlane, frustum[FrustumPlane::Far].distance, epsilon);
}
