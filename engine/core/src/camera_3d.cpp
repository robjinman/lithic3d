#include "lithic3d/camera_3d.hpp"
#include "lithic3d/units.hpp"

namespace lithic3d
{

Camera3d::Camera3d(float aspectRatio, float rotation, float drawDistance)
  : m_drawDistance(drawDistance)
  , m_position{0, 0, 0}
  , m_direction{0, 0, -1}
{
  m_viewMatrix = lookAt(m_position, m_position + m_direction);
  updateParameters(aspectRatio, rotation);
  m_worldSpaceFrustum = computeFrustumFromMatrix(m_projection * m_viewMatrix);
}

void Camera3d::updateParameters(float aspectRatio, float rotation)
{
  float vFov = degreesToRadians(45.f);
  float nearPlane = metresToWorldUnits(0.1f);
  float farPlane = metresToWorldUnits(m_drawDistance);

  Mat4x4f rot = rotationMatrix4x4(Vec3f{ 0.f, 0.f, rotation });
  m_projection = rot * perspective(vFov, aspectRatio, nearPlane, farPlane);

  m_worldSpaceFrustum = computeFrustumFromMatrix(m_projection * m_viewMatrix);
}

float Camera3d::drawDistance() const
{
  return m_drawDistance;
}

void Camera3d::setTransform(const Mat4x4f& transform)
{
  auto rotation = getRotation3x3(transform);
  m_direction = rotation * Vec3f{ 0.f, 0.f, -1.f };
  m_position = getTranslation(transform);

  onMove();
}

void Camera3d::setPosition(const Vec3f& position)
{
  m_position = position;
  onMove();
}

void Camera3d::translate(const Vec3f& delta)
{
  m_position += delta;
  onMove();
}

void Camera3d::rotate(float deltaPitch, float deltaYaw)
{
  // Right-hand rule for cross-product and rotations
  auto pitch = rotationMatrix3x3(m_direction.cross(Vec3f{0, 1, 0}), deltaPitch);
  auto yaw = rotationMatrix3x3(Vec3f{0, 1, 0}, -deltaYaw);
  m_direction = (yaw * pitch * m_direction).normalise();

  onMove();
}

void Camera3d::onMove()
{
  m_viewMatrix = lookAt(m_position, m_position + m_direction);
  m_worldSpaceFrustum = computeFrustumFromMatrix(m_projection * m_viewMatrix);
}

} // namespace lithic3d
