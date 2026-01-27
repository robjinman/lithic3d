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
  // TODO: Why -1?
  return lookAt(m_position, m_position + Vec3f{0, 0, -1});
}

Mat4x4f Camera2d::getProjectionMatrix() const
{
  return m_projection;
}

void Camera2d::updateParameters(float aspectRatio, float rotation)
{
  Mat4x4f m;
  const float t = 0.5f;
  const float b = -0.5f;
  const float r = 0.5f * aspectRatio;
  const float l = -0.5f * aspectRatio;
  const float f = 100.f;
  const float n = 0.f;

  m.set(0, 0, 2.f / (r - l));
  m.set(0, 3, (r + l) / (l - r));
  m.set(1, 1, 2.f / (b - t));
  m.set(1, 3, (b + t) / (b - t));
  m.set(2, 2, 1.f / (f - n));
  m.set(2, 3, -n / (f - n));
  m.set(3, 3, 1.f);

  Mat4x4f rot = rotationMatrix4x4(Vec3f{ 0.f, 0.f, rotation });
  m_projection = rot * m;
}

const Vec3f& Camera2d::getPosition() const
{
  return m_position;
}

} // namespace lithic3d
