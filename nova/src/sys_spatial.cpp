#include "sys_spatial.hpp"
#include "graph.hpp"
#include <unordered_map>
#include <cstring>

namespace
{

class SysSpatialImpl : public SysSpatial
{
  public:
    SysSpatialImpl(ComponentStore& componentStore, EventSystem& eventSystem);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override {}

    void addEntity(EntityId entityId, const SpatialData& data) override;
    EntityId root() const override;
    void setEnabled(EntityId entityId, bool enabled) override;

  private:
    ComponentStore& m_componentStore;
    EventSystem& m_eventSystem;
    GraphPtr<EntityId, NULL_ENTITY> m_sceneGraph;
};

SysSpatialImpl::SysSpatialImpl(ComponentStore& componentStore, EventSystem& eventSystem)
  : m_componentStore(componentStore)
  , m_eventSystem(eventSystem)
{
  EntityId root = m_componentStore.allocate<CLocalTransform, CGlobalTransform, CSpatialFlags>();
  m_sceneGraph = std::make_unique<Graph<EntityId, NULL_ENTITY>>(root);
}

EntityId SysSpatialImpl::root() const
{
  return m_sceneGraph->dfs[0];
}

void SysSpatialImpl::addEntity(EntityId entityId, const SpatialData& data)
{
  auto& parentFlags = m_componentStore.component<CSpatialFlags>(data.parent);

  m_sceneGraph->addItem(entityId, data.parent);
  m_componentStore.component<CLocalTransform>(entityId).transform = data.transform;
  m_componentStore.component<CSpatialFlags>(entityId) = CSpatialFlags{
    .enabled = data.enabled,
    .parentEnabled = parentFlags.parentEnabled && parentFlags.enabled
  };
}

void SysSpatialImpl::removeEntity(EntityId entityId)
{
  m_sceneGraph->removeItem(entityId);
}

bool SysSpatialImpl::hasEntity(EntityId entityId) const
{
  return m_sceneGraph->hasItem(entityId);
}

void SysSpatialImpl::update(Tick, const InputState&)
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

void SysSpatialImpl::setEnabled(EntityId entityId, bool enabled)
{
  auto& node = *m_sceneGraph->nodes.at(entityId);
  auto& flags = m_componentStore.component<CSpatialFlags>(entityId);

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
      auto& parentFlags = m_componentStore.component<CSpatialFlags>(parentId);
      parentEnabled = parentFlags.parentEnabled && parentFlags.enabled;
      prevParentId = parentId;
    }

    auto& descendentFlags = m_componentStore.component<CSpatialFlags>(entityId);
    descendentFlags.parentEnabled = parentEnabled;
  }

  if (enabled) {
    m_eventSystem.queueEvent(std::make_unique<EEntityEnable>(entityId, EntityIdSet{ entityId }));
  }
}

} // namespace

SysSpatialPtr createSysSpatial(ComponentStore& componentStore, EventSystem& eventSystem)
{
  return std::make_unique<SysSpatialImpl>(componentStore, eventSystem);
}
