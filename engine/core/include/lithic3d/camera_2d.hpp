#pragma once

#include "math.hpp"

namespace lithic3d
{

class Camera2d
{
  public:
    Camera2d();

    void setPosition(const Vec3f& position);
    void translate(const Vec3f& delta);
    const Vec3f& getPosition() const;
    Mat4x4f  getMatrix() const;

  private:
    Vec3f m_position;
};

} // namespace lithic3d
