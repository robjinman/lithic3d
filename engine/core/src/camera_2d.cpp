#include "lithic3d/camera_2d.hpp"

namespace lithic3d
{

Camera2d::Camera2d(float aspectRatio, float rotation)
  : m_position{0, 0, 0}
{
  updateParameters(aspectRatio, rotation);
}

void Camera2d::setPosition(const Vec3f& position)
{
  m_position = position;
}

void Camera2d::translate(const Vec3f& delta)
{
  m_position += delta;
}

Mat4x4f Camera2d::getViewMatrix() const
{
  return lookAt(m_position, m_position + Vec3f{0, 0, -1});
}

Mat4x4f Camera2d::getProjectionMatrix() const
{
  return m_projection;
}

void Camera2d::updateParameters(float aspectRatio, float rotation)
{
  const float t = 0.5f;
  const float b = -0.5f;
  const float r = 0.5f * aspectRatio;
  const float l = -0.5f * aspectRatio;
  const float f = 100.f;
  const float n = 0.1f;

  auto m = orthographic(l, r, t, b, n, f);

  Mat4x4f rot = rotationMatrix4x4(Vec3f{ 0.f, 0.f, rotation });
  m_projection = rot * m;
}

const Vec3f& Camera2d::getPosition() const
{
  return m_position;
}

} // namespace lithic3d
