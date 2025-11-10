#include "fge/camera_2d.hpp"

namespace fge
{

Camera2d::Camera2d()
  : m_position{0, 0, 0}
{
}

void Camera2d::setPosition(const Vec3f& position)
{
  m_position = position;
}

void Camera2d::translate(const Vec3f& delta)
{
  m_position += delta;
}

Mat4x4f Camera2d::getMatrix() const
{
  return lookAt(m_position, m_position + Vec3f{0, 0, -1});
}

const Vec3f& Camera2d::getPosition() const
{
  return m_position;
}

} // namespace fge
