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

std::string frustumPlaneName(size_t planeIndex)
{
  switch (planeIndex) {
    case FrustumPlane::Left: return "Left";
    case FrustumPlane::Right: return "Right";
    case FrustumPlane::Bottom: return "Bottom";
    case FrustumPlane::Top: return "Top";
    case FrustumPlane::Near: return "Near";
    case FrustumPlane::Far: return "Far";
  }
  return "";
}

std::ostream& operator<<(std::ostream& stream, const Plane& plane)
{
  stream << plane.normal << " " << plane.distance << std::endl;
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Frustum f)
{
  stream << "---Frustum---" << std::endl;
  stream << frustumPlaneName(FrustumPlane::Near) << ": " << f[FrustumPlane::Near];
  stream << frustumPlaneName(FrustumPlane::Far) << ": " << f[FrustumPlane::Far];
  stream << frustumPlaneName(FrustumPlane::Left) << ": " << f[FrustumPlane::Left];
  stream << frustumPlaneName(FrustumPlane::Right) << ": " << f[FrustumPlane::Right];
  stream << frustumPlaneName(FrustumPlane::Bottom) << ": " << f[FrustumPlane::Bottom];
  stream << frustumPlaneName(FrustumPlane::Top) << ": " << f[FrustumPlane::Top];

  return stream;
}

bool similar(const Vec3f& expected, const Vec3f& actual, float epsilon)
{
  bool result = fabs(actual[0] - expected[0]) <= epsilon &&
    fabs(actual[1] - expected[1]) <= epsilon &&
    fabs(actual[2] - expected[2]) <= epsilon;

  if (!result) {
    EXPECT_NEAR(expected[0], actual[0], epsilon);
    EXPECT_NEAR(expected[1], actual[1], epsilon);
    EXPECT_NEAR(expected[2], actual[2], epsilon);
  }

  return result;
}

TEST_F(Camera3dTest, computeFrustum_camera_at_origin_square_aspect)
{
  float aspectRatio = 1.f;
  Camera3d camera{aspectRatio, 0.f, 1000.f};

  float vFov = degreesToRadians(45.f);
  float hFov = 2.f * atanf(aspectRatio * tanf(0.5f * vFov));
  float nearPlane = metresToWorldUnits(0.1f);
  float farPlane = metresToWorldUnits(1000.f);

  auto frustum = camera.getWorldSpaceFrustum();

  ASSERT_TRUE(dbg_isValidFrustum(frustum));

  // For normals pointing toward the origin, distances should be positive and vice-versa
  // Normals should point toward frustum centre (not necessarily toward origin)
  // normal * -distance should give point on the plane
  // Remember, camera looks down negative z-axis

  EXPECT_TRUE(similar({ 0, 0, -1 }, frustum[FrustumPlane::Near].normal, 0.1f));
  EXPECT_NEAR(-nearPlane, frustum[FrustumPlane::Near].distance, 0.1f);
  EXPECT_TRUE(similar({ 0, 0, 1 }, frustum[FrustumPlane::Far].normal, 0.1f));
  EXPECT_NEAR(farPlane, frustum[FrustumPlane::Far].distance, 1.f);  // Large epsilon
  EXPECT_TRUE(similar(Vec3f{ nearPlane, 0.f, -nearPlane * tanf(hFov * 0.5f) }.normalise(),
    frustum[FrustumPlane::Left].normal, 0.1f));
  EXPECT_NEAR(0.f, frustum[FrustumPlane::Left].distance, 0.1f);
  EXPECT_TRUE(similar(Vec3f{ -nearPlane, 0.f, -nearPlane * tanf(hFov * 0.5f) }.normalise(),
    frustum[FrustumPlane::Right].normal, 0.1f));
  EXPECT_NEAR(0.f, frustum[FrustumPlane::Right].distance, 0.1f);
  EXPECT_TRUE(similar(Vec3f{ 0.f, -nearPlane, -nearPlane * tanf(vFov * 0.5f) }.normalise(),
    frustum[FrustumPlane::Top].normal, 0.1f));
  EXPECT_NEAR(0.f, frustum[FrustumPlane::Top].distance, 0.1f);
  EXPECT_TRUE(similar(Vec3f{ 0.f, nearPlane, -nearPlane * tanf(vFov * 0.5f) }.normalise(),
    frustum[FrustumPlane::Bottom].normal, 0.1f));
  EXPECT_NEAR(0.f, frustum[FrustumPlane::Bottom].distance, 0.1f);
}

TEST_F(Camera3dTest, computeFrustum_returns_valid_frustum_at_various_orientations)
{
  float aspectRatio = 1.4f;
  Camera3d camera{aspectRatio, 0.f, 1000.f};

  camera.translate({ -5.f, 1.f, -5.f });
  camera.rotate(degreesToRadians(-45.f), 0.f);

  for (size_t x = 0; x < 2; ++x) {
    camera.translate({ 10.f * x, 0.f, 0.f });

    for (size_t z = 0; z < 2; ++z) {
      camera.translate({ 0.f, 0.f, 10.f * z });

      for (size_t p = 0; p < 3; ++p) {
        camera.rotate(degreesToRadians(45.f), 0.f);

        for (size_t y = 0; y < 8; ++y) {
          camera.rotate(0.f, degreesToRadians(45.f));

          auto frustum = camera.getWorldSpaceFrustum();
          EXPECT_TRUE(dbg_isValidFrustum(frustum));
        }
      }
    }
  }
}
