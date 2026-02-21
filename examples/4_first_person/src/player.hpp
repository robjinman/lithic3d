#pragma once

#include <lithic3d/lithic3d.hpp>
#include <memory>

class Player
{
  public:
    virtual const lithic3d::Vec3f& getPosition() const = 0;
    virtual void setPosition(const lithic3d::Vec3f& position) = 0;
    virtual lithic3d::Vec3f getDirection() const = 0;
    virtual void translate(const lithic3d::Vec3f& delta) = 0;
    virtual void rotate(float deltaPitch, float deltaYaw) = 0;  // Yaw is rotation to the right
    virtual float getStepHeight() const = 0;
    virtual float getSpeed() const = 0;
    virtual float getRadius() const = 0;
    virtual float getJumpHeight() const = 0;

    virtual ~Player() {}
};

using PlayerPtr = std::unique_ptr<Player>;

PlayerPtr createPlayer(lithic3d::Camera3d& camera);
