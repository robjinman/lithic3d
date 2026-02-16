#include "lithic3d/camera_3d.hpp"
#include "lithic3d/units.hpp"

namespace lithic3d
{

Camera3d::Camera3d(float aspectRatio, float rotation)
  : m_position{0, 0, 0}
  , m_direction{0, 0, -1}
{
  updateParameters(aspectRatio, rotation);
  m_viewMatrix = lookAt(m_position, m_position + m_direction);
  //m_viewSpaceFrustum = computeFrustum(m_projection);
  m_worldSpaceFrustum = computeFrustum(m_projection * m_viewMatrix);
}

void Camera3d::updateParameters(float aspectRatio, float rotation)
{
  float vFov = degreesToRadians(45.f);
  float hFov = 2.f * atanf(aspectRatio * tanf(0.5f * vFov));
  float nearPlane = metresToWorldUnits(0.1f);
  float farPlane = metresToWorldUnits(1000.f);

  Mat4x4f rot = rotationMatrix4x4(Vec3f{ 0.f, 0.f, rotation });
  m_projection = rot * perspective(vFov, aspectRatio, nearPlane, farPlane);

  //m_viewSpaceFrustum = computeFrustum(m_projection);
  m_worldSpaceFrustum = computeFrustum(m_projection * m_viewMatrix);
}

Frustum Camera3d::computeFrustum(const Mat4x4f& m) const
{
  auto calcPlane = [](const Mat4x4f& m, size_t rowA, size_t rowB, bool add) {
    auto v = add ? m.row(rowA) + m.row(rowB) : m.row(rowA) - m.row(rowB);
    auto normal = v.sub<3>();
    
    Plane plane{
      .normal = normal,
      .distance = v[3]
    };
    plane.normalise();

    return plane;
  };

  Frustum frustum;

  frustum[FrustumPlane::Left] = calcPlane(m, 3, 0, true);     // row 3 + row 0
  frustum[FrustumPlane::Right] = calcPlane(m, 3, 0, false);   // row 3 - row 0
  frustum[FrustumPlane::Bottom] = calcPlane(m, 3, 1, false);  // row 3 - row 1
  frustum[FrustumPlane::Top] = calcPlane(m, 3, 1, true);      // row 3 + row 1
  frustum[FrustumPlane::Far] = calcPlane(m, 3, 2, false);     // row 3 - row 2

  auto normal = m.row(2).sub<3>();
  frustum[FrustumPlane::Near] = Plane{                        // row 2
    .normal = normal,
    .distance = m.at(2, 3)
  };
  frustum[FrustumPlane::Near].normalise();

  return frustum;
}

void Camera3d::setPosition(const Vec3f& position)
{
  m_position = position;
  m_viewMatrix = lookAt(m_position, m_position + m_direction);
  m_worldSpaceFrustum = computeFrustum(m_projection * m_viewMatrix);
}

void Camera3d::translate(const Vec3f& delta)
{
  m_position += delta;
  m_viewMatrix = lookAt(m_position, m_position + m_direction);
  m_worldSpaceFrustum = computeFrustum(m_projection * m_viewMatrix);
}

void Camera3d::rotate(float deltaPitch, float deltaYaw)
{
  // Right-hand rule for cross-product and rotations
  auto pitch = rotationMatrix3x3(m_direction.cross(Vec3f{0, 1, 0}), deltaPitch);
  auto yaw = rotationMatrix3x3(Vec3f{0, 1, 0}, -deltaYaw);
  m_direction = (yaw * pitch * m_direction).normalise();

  m_viewMatrix = lookAt(m_position, m_position + m_direction);
  m_worldSpaceFrustum = computeFrustum(m_projection * m_viewMatrix);
}

} // namespace lithic3d
