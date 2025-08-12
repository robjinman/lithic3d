#include "behaviours.hpp"
#include "world_grid.hpp"
#include "entity_manager.hpp"
#include "c_render.hpp"

class CollectableBehaviour : public Behaviour
{
  public:
    CollectableBehaviour(EntityId entityId, EntityId playerId, const WorldGrid& worldGrid,
      EntityManager& entityManager, int value)
      : m_worldGrid(worldGrid)
      , m_entityManager(entityManager)
      , m_entityId(entityId)
      , m_playerId(playerId)
      , m_value(value)
    {}

    void handleEvent(const Event& event) override;

  private:
    const WorldGrid& m_worldGrid;
    EntityManager& m_entityManager;
    EntityId m_entityId;
    EntityId m_playerId;
    int m_value = 0;
};

void CollectableBehaviour::handleEvent(const Event& event)
{
  static hashedString_t strPlayerMoved = hashString("player_moved");
  static hashedString_t strAnimFinished = hashString("animation_finished");
  static hashedString_t strRender = hashString("render");
  static hashedString_t strCollect = hashString("collect");

  if (event.name() == strPlayerMoved) {
    auto& e = dynamic_cast<const EPlayerMoved&>(event);

    if (m_worldGrid.hasEntityAt(e.toPos[0], e.toPos[1], m_entityId)) {
      auto& entity = m_entityManager.getEntity(m_entityId);
      auto& cRender = dynamic_cast<CRender&>(entity.getComponent(strRender));

      cRender.playAnimation(strCollect);
      m_entityManager.notify(m_playerId, EItemCollected{m_entityId, m_value});
    }
  }
  else if (event.name() == strAnimFinished) {
    m_entityManager.deleteEntity(m_entityId);
  }
}

BehaviourPtr createCollectableBehaviour(EntityId entityId, EntityId playerId,
  const WorldGrid& worldGrid, EntityManager& entityManager, int value)
{
  return std::make_unique<CollectableBehaviour>(entityId, playerId, worldGrid, entityManager,
    value);
}
