#pragma once

#include <lithic3d/lithic3d.hpp>
#include <memory>

class Player
{
  public:
    virtual void update() = 0;
    virtual const lithic3d::Mat4x4f& getTransform() const = 0;
    virtual lithic3d::Vec3f getPosition() const = 0;
    virtual void setPosition(const lithic3d::Vec3f& position) = 0;
    virtual lithic3d::Vec3f getDirection() const = 0;
    virtual void translate(const lithic3d::Vec3f& delta) = 0;
    virtual void rotate(float deltaPitch, float deltaYaw) = 0;  // Yaw is rotation to the right
    virtual lithic3d::EntityId getId() const = 0;

    virtual ~Player() {}
};

using PlayerPtr = std::unique_ptr<Player>;

PlayerPtr createPlayer(lithic3d::Ecs& ecs, lithic3d::RenderResourceLoader& renderResourceLoader,
  lithic3d::ModelLoader& modelLoader);
