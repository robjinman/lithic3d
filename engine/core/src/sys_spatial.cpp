#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/graph.hpp"
#include <unordered_map>
#include <array>
#include <cstring>

namespace lithic3d
{
namespace
{

class SysSpatialImpl : public SysSpatial
{
  public:
    SysSpatialImpl(Ecs& ecs, EventSystem& eventSystem);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override {}

    void transformEntity(EntityId id, const Mat4x4f& m) override;
    void setEntityTransform(EntityId id, const Mat4x4f& m) override;

    EntityIdSet getIntersecting(const Frustum& frustum) const override;

    void addEntity(EntityId entityId, const DSpatial& data) override;
    EntityId root() const override;
    void setEnabled(EntityId entityId, bool enabled) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    GraphPtr<EntityId, NULL_ENTITY_ID> m_sceneGraph;
    LooseOctreePtr m_octree;

    void updateBoundingBox(EntityId entityId, const Mat4x4f& m);
};

SysSpatialImpl::SysSpatialImpl(Ecs& ecs, EventSystem& eventSystem)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
{
  EntityId root = m_ecs.idGen().getNewEntityId();
  m_ecs.componentStore().allocate<DSpatial>(id);
  m_sceneGraph = std::make_unique<Graph<EntityId, NULL_ENTITY_ID>>(root);
  m_octree = createLooseOctree({ 0.f, -100.f, 0.f }, 10000.f); // TODO
}

EntityIdSet SysSpatialImpl::getIntersecting(const Frustum& frustum) const
{
  return m_octree->getIntersecting(frustum);
}

EntityId SysSpatialImpl::root() const
{
  return m_sceneGraph->dfs[0];
}

void SysSpatialImpl::addEntity(EntityId entityId, const DSpatial& data)
{
  auto& parentFlags = m_ecs.componentStore().component<CSpatialFlags>(data.parent);

  m_sceneGraph->addItem(entityId, data.parent);
  m_ecs.componentStore().component<CLocalTransform>(entityId).transform = data.transform;
  m_ecs.componentStore().component<CSpatialFlags>(entityId) = CSpatialFlags{
    .enabled = data.enabled,
    .parentEnabled = parentFlags.parentEnabled && parentFlags.enabled
  };
  m_ecs.componentStore().component<CBoundingBox>(entityId).modelSpaceAabb = data.aabb;
}

void SysSpatialImpl::removeEntity(EntityId entityId)
{
  m_sceneGraph->removeItem(entityId);
  m_octree->remove(entityId);
}

bool SysSpatialImpl::hasEntity(EntityId entityId) const
{
  return m_sceneGraph->hasItem(entityId);
}

void SysSpatialImpl::transformEntity(EntityId id, const Mat4x4f& m)
{
  auto& c = m_ecs.componentStore().component<CLocalTransform>(id);
  c.transform = m * c.transform;
}

void SysSpatialImpl::setEntityTransform(EntityId id, const Mat4x4f& m)
{
  auto& c = m_ecs.componentStore().component<CLocalTransform>(id);
  c.transform = m;
}

void SysSpatialImpl::updateBoundingBox(EntityId entityId, const Mat4x4f& m)
{
  auto& box = m_ecs.componentStore().component<CBoundingBox>(entityId);

  Vec3f d = box.modelSpaceAabb.max - box.modelSpaceAabb.min;
  std::array<Vec3f, 8> points;

  for (uint32_t i = 0; i < 8; ++i) {
    Vec3f P = box.modelSpaceAabb.min + Vec3f{
      i & 0b001 ? d[0] : 0.f,
      i & 0b010 ? d[1] : 0.f,
      i & 0b100 ? d[2] : 0.f
    };

    points[i] = (m * Vec4f{ P[0], P[1], P[2], 1.f }).sub<3>();
  }

  box.worldSpaceAabb.min = {
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max()
  };
  box.worldSpaceAabb.max = {
    std::numeric_limits<float>::lowest(),
    std::numeric_limits<float>::lowest(),
    std::numeric_limits<float>::lowest()
  };

  for (size_t i = 0; i < 8; ++i) {
    for (uint32_t j = 0; j < 3; ++j) {
      if (points[i][j] < box.worldSpaceAabb.min[j]) {
        box.worldSpaceAabb.min[j] = points[i][j];
      }
      if (points[i][j] > box.worldSpaceAabb.max[j]) {
        box.worldSpaceAabb.max[j] = points[i][j];
      }
    }
  }

  Vec3f pos = box.worldSpaceAabb.min + (box.worldSpaceAabb.max - box.worldSpaceAabb.min) * 0.5f;
  float radius = (box.worldSpaceAabb.max - box.worldSpaceAabb.min).magnitude() * 0.5f;

  m_octree->move(entityId, pos, radius);
}

void SysSpatialImpl::update(Tick, const InputState&)
{
  Mat4x4f I = identityMatrix<4>();

  Mat4x4f* parentT = &I;
  EntityId prevParentId = NULL_ENTITY_ID;

  // Skip the root
  for (size_t i = 1; i < m_sceneGraph->dfs.size(); ++i) {
    auto entityId = m_sceneGraph->dfs[i];
    auto parentId = m_sceneGraph->parents[i];

    if (parentId != prevParentId) {
      parentT = &m_ecs.componentStore().component<CGlobalTransform>(parentId).transform;
      prevParentId = parentId;
    }

    // TODO: Possibly slow std::map lookups, negates benefit of compact buffer
    auto& localT = m_ecs.componentStore().component<CLocalTransform>(entityId);
    auto& globalT = m_ecs.componentStore().component<CGlobalTransform>(entityId);

    // TODO: Skip if entity is static or local transform is non-dirty

    globalT.transform = *parentT * localT.transform;

    updateBoundingBox(entityId, globalT.transform);
  }
}

void SysSpatialImpl::setEnabled(EntityId entityId, bool enabled)
{
  auto& node = *m_sceneGraph->nodes.at(entityId);
  auto& flags = m_ecs.componentStore().component<CSpatialFlags>(entityId);

  if (flags.enabled == enabled) {
    return;
  }

  flags.enabled = enabled;

  EntityId prevParentId = entityId;
  bool parentEnabled = flags.parentEnabled && flags.enabled;

  // Propagate flags to descendents
  for (size_t i = node.index + 1; i <= node.index + node.numDescendents; ++i) {
    auto entityId = m_sceneGraph->dfs[i];
    auto parentId = m_sceneGraph->parents[i];

    if (parentId != prevParentId) {
      auto& parentFlags = m_ecs.componentStore().component<CSpatialFlags>(parentId);
      parentEnabled = parentFlags.parentEnabled && parentFlags.enabled;
      prevParentId = parentId;
    }

    auto& descendentFlags = m_ecs.componentStore().component<CSpatialFlags>(entityId);
    descendentFlags.parentEnabled = parentEnabled;
  }

  if (enabled) {
    m_eventSystem.raiseEvent(EEntityEnable{entityId, EntityIdSet{ entityId }});
  }
}

} // namespace

SysSpatialPtr createSysSpatial(Ecs& ecs, EventSystem& eventSystem)
{
  return std::make_unique<SysSpatialImpl>(ecs, eventSystem);
}

} // namespace lithic3d
