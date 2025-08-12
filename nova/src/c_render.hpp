#pragma once

#include "component.hpp"

class Animation
{

};

using AnimationPtr = std::unique_ptr<Animation>;

class CRender : public Component
{
  public:
    void update() override;

    virtual void addAnimation(AnimationPtr animation) = 0;
    virtual void playAnimation(hashedString_t name) = 0;
    virtual void render() = 0;
};

using CRenderPtr = std::unique_ptr<CRender>;

class Renderer;

CRenderPtr createRenderComponent(Renderer& renderer);
