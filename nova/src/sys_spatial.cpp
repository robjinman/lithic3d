#include "sys_spatial.hpp"
#include "graph.hpp"
#include <unordered_map>
#include <cstring>

namespace
{

class SysSpatialImpl : public SysSpatial
{
  public:
    SysSpatialImpl(ComponentStore& componentStore);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override;
    void processEvent(const Event& event) override {}

    void addEntity(EntityId entityId, const CSpatial& data) override;
    EntityId root() const override;
    void setFlags(EntityId entityId, bool active) override;

  private:
    ComponentStore& m_componentStore;
    GraphPtr<EntityId, NULL_ENTITY> m_sceneGraph;
};

SysSpatialImpl::SysSpatialImpl(ComponentStore& componentStore)
  : m_componentStore(componentStore)
{
  EntityId root = m_componentStore.allocate<CLocalTransform, CGlobalTransform, CSpatialFlags>();
  m_sceneGraph = std::make_unique<Graph<EntityId, NULL_ENTITY>>(root);
}

EntityId SysSpatialImpl::root() const
{
  return m_sceneGraph->dfs[0];
}

void SysSpatialImpl::addEntity(EntityId entityId, const CSpatial& data)
{
  m_sceneGraph->addItem(entityId, data.parent);
  m_componentStore.component<CLocalTransform>(entityId).transform = data.transform;
  m_componentStore.component<CSpatialFlags>(entityId).active = data.isActive;
}

void SysSpatialImpl::removeEntity(EntityId entityId)
{
  m_sceneGraph->removeItem(entityId);
}

bool SysSpatialImpl::hasEntity(EntityId entityId) const
{
  return m_sceneGraph->hasItem(entityId);
}

void SysSpatialImpl::update(Tick)
{
  Mat4x4f I = identityMatrix<float_t, 4>();

  Mat4x4f* parentT = &I;
  EntityId prevParentId = NULL_ENTITY;

  // Skip the root
  for (size_t i = 1; i < m_sceneGraph->dfs.size(); ++i) {
    auto entityId = m_sceneGraph->dfs[i];
    auto parentId = m_sceneGraph->parents[i];

    if (parentId != prevParentId) {
      parentT = &m_componentStore.component<CGlobalTransform>(parentId).transform;
      prevParentId = parentId;
    }

    auto& localT = m_componentStore.component<CLocalTransform>(entityId);
    auto& globalT = m_componentStore.component<CGlobalTransform>(entityId);

    globalT.transform = *parentT * localT.transform;
  }
}

void SysSpatialImpl::setFlags(EntityId entityId, bool active)
{
  auto& node = *m_sceneGraph->nodes.at(entityId);

  for (size_t i = node.index; i <= node.index + node.numDescendents; ++i) {
    auto id = m_sceneGraph->dfs[i];
    auto& flags = m_componentStore.component<CSpatialFlags>(id);
    flags.active = active;
  }
}

} // namespace

SysSpatialPtr createSysSpatial(ComponentStore& componentStore)
{
  return std::make_unique<SysSpatialImpl>(componentStore);
}
