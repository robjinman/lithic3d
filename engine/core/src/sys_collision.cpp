#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"

namespace lithic3d
{
namespace
{

using CollisionPair = std::pair<EntityId, EntityId>;

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DCollision& data) override;
    void addEntity(EntityId id, const DTerrainChunk data) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;

    void applyGravity();
    std::vector<CollisionPair> findPossibleCollisions();

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    Ecs& m_ecs;
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

void SysCollisionImpl::addEntity(EntityId id, const DCollision& data)
{

}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
  };
}

void SysCollisionImpl::removeEntity(EntityId entityId)
{

}

bool SysCollisionImpl::hasEntity(EntityId entityId) const
{

}

void SysCollisionImpl::applyGravity()
{

}

std::vector<CollisionPair> SysCollisionImpl::findPossibleCollisions()
{
  // Extremely naive collision algorithm
  // TODO: Use something like sweep & prune instead: https://leanrada.com/notes/sweep-and-prune/

  std::vector<CollisionPair> pairs;

  auto groups = m_ecs.componentStore().components<CSpatialFlags, CGlobalTransform, CCollision>();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto boundingBoxes = group.components<CBoundingBox>();
    //auto collisionComps = group.components<CCollision>();
    auto entityIds = group.entityIds();

    for (size_t i = 0; i < entityIds.size(); ++i) {
      auto& flagsI = flagsComps[i].flags;

      if (flagsI.test(SpatialFlags::ParentEnabled) && flagsI.test(SpatialFlags::Enabled)) {
        for (size_t j = i + 1; j < entityIds.size(); ++j) {
          auto& flagsJ = flagsComps[j].flags;

          if (flagsJ.test(SpatialFlags::ParentEnabled) && flagsJ.test(SpatialFlags::Enabled)) {
            auto& box1 = boundingBoxes[i].worldSpaceAabb;
            auto& box2 = boundingBoxes[j].worldSpaceAabb;

            if (box1.max[0] >= box2.min[0] && box1.min[0] <= box2.max[0] &&
              box1.max[1] >= box2.min[1] && box1.min[1] <= box2.max[1] &&
              box1.max[2] >= box2.min[2] && box1.min[2] <= box2.max[2]) {

              pairs.push_back({ entityIds[i], entityIds[j] });
            }
          }
        }
      }
    }
  }

  return pairs;
}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{
  applyGravity();

  auto pairs = findPossibleCollisions();

  for (auto& pair : pairs) {
    m_logger.info(STR("Collision! " << pair.first << ", " << pair.second));
  }
}

void SysCollisionImpl::processEvent(const Event& event)
{

}

} // namespace

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysCollisionImpl>(ecs, eventSystem, logger);
}

} // namespace lithic3d
