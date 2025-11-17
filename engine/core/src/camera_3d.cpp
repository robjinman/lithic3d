#include "lithic3d/camera_3d.hpp"

namespace lithic3d
{

Camera3d::Camera3d()
  : m_position{0, 0, 0}
  , m_direction{0, 0, 1}
{
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

Mat4x4f Camera3d::getMatrix() const
{
  return lookAt(m_position, m_position + m_direction);
}

const Vec3f& Camera3d::getPosition() const
{
  return m_position;
}

const Vec3f& Camera3d::getDirection() const
{
  return m_direction;
}

} // namespace lithic3d
