#pragma once

#include "math.hpp"

namespace lithic3d
{

class Camera2d
{
  public:
    Camera2d(float aspectRatio, float rotation);

    void setPosition(const Vec3f& position);
    void translate(const Vec3f& delta);
    const Vec3f& getPosition() const;
    Mat4x4f getViewMatrix() const;
    Mat4x4f getProjectionMatrix() const;
    void updateParameters(float aspectRatio, float rotation);

  private:
    Vec3f m_position;
    Mat4x4f m_projection;
};

} // namespace lithic3d
