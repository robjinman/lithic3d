#pragma once

#include "renderables.hpp"

class Animation
{

};

using AnimationPtr = std::unique_ptr<Animation>;

class CRender : public Component
{
  public:
    void update() override;

    virtual void addAnimation(AnimationPtr animation) = 0;
    virtual void playAnimation(HashedString name) = 0;
    virtual void render() = 0;
};

using CRenderPtr = std::unique_ptr<CRender>;

class Renderer;

CRenderPtr createRenderComponent(Renderer& renderer, RenderItemId texture, const Rectf& textureRect,
  const Vec2f& size, const Vec2f& pos, uint32_t zIndex);
