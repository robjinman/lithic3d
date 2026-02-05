#pragma once

#include "math.hpp"

namespace lithic3d
{

class Camera3d
{
  public:
    Camera3d(float aspectRatio, float rotation);

    void setPosition(const Vec3f& position);
    void translate(const Vec3f& delta);
    void rotate(float deltaPitch, float deltaYaw); // Yaw is rotation to the right
    inline const Vec3f& getDirection() const;
    inline const Vec3f& getPosition() const;
    Mat4x4f getViewMatrix() const;
    inline const Mat4x4f& getProjectionMatrix() const;
    Frustum computeFrustum() const;
    void updateParameters(float aspectRatio, float rotation);

  private:
    Vec3f m_position;
    Vec3f m_direction;
    Mat4x4f m_projection;
};

const Vec3f& Camera3d::getPosition() const
{
  return m_position;
}

const Vec3f& Camera3d::getDirection() const
{
  return m_direction;
}

const Mat4x4f& Camera3d::getProjectionMatrix() const
{
  return m_projection;
}

} // namespace lithic3d
