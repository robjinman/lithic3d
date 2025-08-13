#include "c_collectable.hpp"
#include "c_render.hpp"
#include "world_grid.hpp"

namespace
{

constexpr HashedString strPlayerMoved = hashString("player_moved");
constexpr HashedString strAnimFinished = hashString("animation_finished");
constexpr HashedString strCollect = hashString("collect");

class CCollectable : public Component
{
  public:
    CCollectable(EntityId entityId, CRender& renderComp, EntityId playerId,
      const WorldGrid& worldGrid, EventSystem& eventSystem, int value)
      : m_worldGrid(worldGrid)
      , m_eventSystem(eventSystem)
      , m_renderComp(renderComp)
      , m_entityId(entityId)
      , m_playerId(playerId)
      , m_value(value)
    {}

    const std::set<HashedString>& subscriptions() const
    {
      static std::set<HashedString> subs{
        strPlayerMoved,
        strAnimFinished
      };
    }

    void update() override {}
    void handleEvent(const GameEvent& event) override;

  private:
    const WorldGrid& m_worldGrid;
    EventSystem& m_eventSystem;
    CRender& m_renderComp;
    EntityId m_entityId;
    EntityId m_playerId;
    int m_value = 0;
};

void CCollectable::handleEvent(const GameEvent& event)
{
  if (event.name == strPlayerMoved) {
    auto& e = dynamic_cast<const EPlayerMoved&>(event);

    if (m_worldGrid.hasEntityAt(e.toPos[0], e.toPos[1], m_entityId)) {
      m_renderComp.playAnimation(strCollect);
      m_eventSystem.fireEvent(EItemCollected{m_entityId, m_value});
    }
  }
  else if (event.name == strAnimFinished) {
    m_eventSystem.fireEvent(ERequestDeletion{m_entityId});
  }
}

}

ComponentPtr createCCollectable(EntityId entityId, CRender& renderComp, EntityId playerId,
  const WorldGrid& worldGrid, EventSystem& eventSystem, int value)
{
  return std::make_unique<CCollectable>(entityId, renderComp, playerId, worldGrid,
    eventSystem, value);
}
