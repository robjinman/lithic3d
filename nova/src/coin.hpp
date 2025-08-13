#pragma once

#include "entity.hpp"

class Coin : public Entity
{
  public:
    Coin(RenderItemId textureAtlas, render::Renderer& renderer, const WorldGrid& worldGrid,
      EventSystem& eventSystem, EntityId playerId)
    {
      m_renderComp = createRenderComponent(renderer, textureAtlas,
        Rectf{ 0, 0, 0.123, 0.234 }, Vec2f{ 0.5, 0.5 }, Vec2f{ 0, 0 }, 5);
      m_collectableComp = createCollectableComponent(id(), *m_renderComp, playerId, worldGrid,
        eventSystem, 1);
    }

    void draw() const override
    {
      m_renderComp->render();
    }

    void update()
    {
      m_renderComp->update();
      m_collectableComp->update();
    }

    void handleEvent(const Event& event)
    {
      m_renderComp->handleEvent(event);
      m_collectableComp->handleEvent(event);
    }

  private:
    CRenderPtr m_renderComp;
    CCollectablePtr m_collectableComp;
};

EntityPtr createCoin(RenderItemId textureAtlas, render::Renderer& renderer,
  const WorldGrid& worldGrid, EventSystem& eventSystem, EntityId playerId);
