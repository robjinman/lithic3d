#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"

namespace lithic3d
{
namespace
{

const Force GravitationalForce{
  .force{ 0.f, -metresToWorldUnits(9.8f) / (TICKS_PER_SECOND * TICKS_PER_SECOND), 0.f },
  .lifetime = std::numeric_limits<uint32_t>::max()
};

using CollisionPair = std::pair<EntityId, EntityId>;

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DCollision& data) override;
    void addEntity(EntityId id, const DTerrainChunk data) override;

    void setInverseMass(EntityId id, float inverseMass) override;
    void applyForce(EntityId id, const Vec3f& force, float seconds) override;
    void setStationary(EntityId id) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    Ecs& m_ecs;

    void applyForce(CCollision& comp, const Force& force);
    std::vector<CollisionPair> findPossibleCollisions();
    void integrate();
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

void SysCollisionImpl::addEntity(EntityId id, const DCollision& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  CCollision comp{
    .inverseMass = data.inverseMass,
    .forces{},
    .acceleration{},
    .velocity{}
  };

  if (comp.inverseMass != 0) {
    comp.forces[0] = GravitationalForce;
  }

  componentStore.instantiate<CCollision>(id) = comp;
}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
  };
}

void SysCollisionImpl::removeEntity(EntityId entityId)
{

}

bool SysCollisionImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

void SysCollisionImpl::applyForce(EntityId id, const Vec3f& force, float seconds)
{
  // TODO
}

void SysCollisionImpl::applyForce(CCollision& comp, const Force& force)
{
  uint32_t freeIndex = MAX_FORCES;
  for (size_t i = 0; i < MAX_FORCES; ++i) {
    if (comp.forces[i].lifetime == 0) {
      freeIndex = i;
    }
  }
  if (freeIndex == MAX_FORCES) {
    m_logger.warn("Max forces exceeded on object");
    return;
  }
  comp.forces[freeIndex] = force;
}

void SysCollisionImpl::setStationary(EntityId id)
{
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
  comp.acceleration = {};
  comp.velocity = {};
  comp.forces = {};

  if (comp.inverseMass != 0) {
    comp.forces[0] = GravitationalForce;
  }
}

void SysCollisionImpl::setInverseMass(EntityId id, float inverseMass)
{
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
  if (comp.inverseMass == 0.f && inverseMass != 0.f) {
    applyForce(comp, GravitationalForce);
  }
  comp.inverseMass = inverseMass;
}

std::vector<CollisionPair> SysCollisionImpl::findPossibleCollisions()
{
  // Extremely naive collision algorithm
  // TODO: Use something like sweep & prune instead: https://leanrada.com/notes/sweep-and-prune/

  std::vector<CollisionPair> pairs;

  auto groups = m_ecs.componentStore().components<CSpatialFlags, CBoundingBox, CCollision>();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto boundingBoxes = group.components<CBoundingBox>();
    //auto collisionComps = group.components<CCollision>();
    auto entityIds = group.entityIds();

    for (size_t i = 0; i < entityIds.size(); ++i) {
      auto& flags = flagsComps[i].flags;

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {
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

void SysCollisionImpl::integrate()
{
  auto groups = m_ecs.componentStore().components<CSpatialFlags, CLocalTransform, CCollision>();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localT = group.components<CLocalTransform>();
    auto collisionComps = group.components<CCollision>();

    for (size_t i = 0; i < flagsComps.size(); ++i) {
      auto& flags = flagsComps[i].flags;
      auto& collision = collisionComps[i];

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {
        Vec3f force;

        for (auto& f : collision.forces) {
          if (f.lifetime > 0) {
            force += f.force;
            --f.lifetime;
          }
        }

        collision.acceleration = force * collision.inverseMass;
        collision.velocity += collision.acceleration;

        localT[i].transform = localT[i].transform * translationMatrix4x4(collision.velocity);
        flags.set(SpatialFlags::Dirty);
      }
    }
  }
}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{
  integrate();

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
