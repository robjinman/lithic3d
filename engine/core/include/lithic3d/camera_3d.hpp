#pragma once

#include "math.hpp"

namespace lithic3d
{

class Camera3d
{
  public:
    Camera3d();

    void setPosition(const Vec3f& position);
    void translate(const Vec3f& delta);
    void rotate(float deltaPitch, float deltaYaw);
    const Vec3f& getDirection() const;
    const Vec3f& getPosition() const;
    Mat4x4f  getMatrix() const;

  private:
    Vec3f m_position;
    Vec3f m_direction;
};

} // namespace lithic3d
