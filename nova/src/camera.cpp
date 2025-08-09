#include "camera.hpp"

Camera::Camera()
  : m_position{0, 0, 0}
{
}

void Camera::setPosition(const Vec3f& position)
{
  m_position = position;
}

void Camera::translate(const Vec3f& delta)
{
  m_position += delta;
}

Mat4x4f Camera::getMatrix() const
{
  return lookAt(m_position, m_position + Vec3f{0, 0, -1});
}

const Vec3f& Camera::getPosition() const
{
  return m_position;
}
