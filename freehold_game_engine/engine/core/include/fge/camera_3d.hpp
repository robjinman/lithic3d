#pragma once

#include "math.hpp"

namespace fge
{

class Camera3d
{
  public:
    Camera3d();

    void setPosition(const Vec3f& position);
    void translate(const Vec3f& delta);
    void rotate(float_t deltaPitch, float_t deltaYaw);
    const Vec3f& getDirection() const;
    const Vec3f& getPosition() const;
    Mat4x4f  getMatrix() const;

  private:
    Vec3f m_position;
    Vec3f m_direction;
};

} // namespace fge
