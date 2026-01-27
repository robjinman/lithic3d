#include "lithic3d/camera_3d.hpp"
#include "lithic3d/units.hpp"

namespace lithic3d
{

Camera3d::Camera3d(float aspectRatio, float rotation)
  : m_position{0, 0, 0}
  , m_direction{0, 0, 1}
{
  updateParameters(aspectRatio, rotation);
}

void Camera3d::updateParameters(float aspectRatio, float rotation)
{
  float vFov = 45.f;
  float hFov = 2.f * atan(aspectRatio * tan(0.5f * vFov));
  float nearPlane = metresToWorldUnits(0.1f);
  float farPlane = metresToWorldUnits(1000.f);

  Mat4x4f rot = rotationMatrix4x4(Vec3f{ 0.f, 0.f, rotation });
  m_projection = rot * perspective(hFov, vFov, nearPlane, farPlane);
}

Frustum Camera3d::computeFrustum() const
{
  auto calcPlane = [](const Mat4x4f& m, size_t rowA, size_t rowB, bool add) {
    auto tmp = add ? m.row(rowA) + m.row(rowB) : m.row(rowA) - m.row(rowB);
    return Plane{
      .normal = tmp.sub<3>(),
      .distance = tmp[3]
    };
  };

  auto m = m_projection * getViewMatrix();

  Frustum frustum;

  frustum[FrustumPlane::Left] = calcPlane(m, 3, 0, true);    // row 3 + row 0
  frustum[FrustumPlane::Right] = calcPlane(m, 3, 0, false);  // row 3 - row 0
  frustum[FrustumPlane::Top] = calcPlane(m, 3, 1, false);    // row 3 - row 1
  frustum[FrustumPlane::Bottom] = calcPlane(m, 3, 1, true);  // row 3 + row 1
  frustum[FrustumPlane::Near] = calcPlane(m, 3, 2, true);    // row 3 + row 2
  frustum[FrustumPlane::Far] = calcPlane(m, 3, 2, false);    // row 3 - row 2

  return frustum;
}

void Camera3d::setPosition(const Vec3f& position)
{
  m_position = position;
}

void Camera3d::translate(const Vec3f& delta)
{
  m_position += delta;
}

void Camera3d::rotate(float deltaPitch, float deltaYaw)
{
  auto pitch = rotationMatrix3x3(m_direction.cross(Vec3f{0, 1, 0}), deltaPitch);
  auto yaw = rotationMatrix3x3(Vec3f{0, 1, 0}, deltaYaw);
  m_direction = (yaw * pitch * m_direction).normalise();
}

Mat4x4f Camera3d::getViewMatrix() const
{
  return lookAt(m_position, m_position + m_direction);
}

} // namespace lithic3d
