#include "sys_spatial.hpp"
#include <unordered_map>
#include <cstring>

namespace
{

struct Node
{
  EntityId parentId = NULL_ENTITY;
  size_t index = 0;
  size_t numDescendents = 0;
};

using NodePtr = std::unique_ptr<Node>;

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

  private:
    ComponentStore& m_componentStore;
    std::vector<EntityId> m_dfsHierarchy;
    std::vector<EntityId> m_parents;
    EntityId m_root;
    std::unordered_map<EntityId, NodePtr> m_entities;

    void incrementDescendentCount(Node& node);
    void decrementDescendentCount(Node& node, size_t n);
};

} // namespace

SysSpatialImpl::SysSpatialImpl(ComponentStore& componentStore)
  : m_componentStore(componentStore)
{
  m_root = m_componentStore.allocate<CLocalTransform, CGlobalTransform, CSpatialFlags>();

  m_dfsHierarchy.push_back(m_root);
  m_parents.push_back(NULL_ENTITY);
  m_entities.insert({ m_root, std::make_unique<Node>() });
}

EntityId SysSpatialImpl::root() const
{
  assert(m_dfsHierarchy.size() > 0);
  return m_dfsHierarchy[0];
}

void SysSpatialImpl::incrementDescendentCount(Node& node)
{
  ++node.numDescendents;
  if (node.parentId != NULL_ENTITY) {
    incrementDescendentCount(*m_entities.at(node.parentId));
  }
}

void SysSpatialImpl::decrementDescendentCount(Node& node, size_t n)
{
  node.numDescendents -= n;
  if (node.parentId != NULL_ENTITY) {
    decrementDescendentCount(*m_entities.at(node.parentId), n);
  }
}

void SysSpatialImpl::addEntity(EntityId entityId, const CSpatial& data)
{
  auto& parentNode = *m_entities.at(data.parent);

  m_componentStore.component<CLocalTransform>(entityId).transform = data.transform;
  m_componentStore.component<CSpatialFlags>(entityId).active = data.isActive;

  incrementDescendentCount(parentNode);
  size_t index = parentNode.index + parentNode.numDescendents;
  size_t numEntities = m_dfsHierarchy.size();

  m_dfsHierarchy.push_back(0);
  m_parents.push_back(0);

  if (index < numEntities) {
    size_t remainder = numEntities - index;
    std::memmove(&m_dfsHierarchy[index + 1], &m_dfsHierarchy[index], remainder * sizeof(EntityId));
    std::memmove(&m_parents[index + 1], &m_parents[index], remainder * sizeof(EntityId));
    assert(m_dfsHierarchy.size() == m_parents.size());
  }

  m_dfsHierarchy[index] = entityId;
  m_parents[index] = data.parent;

  auto node = std::make_unique<Node>(data.parent, index, 0);
  m_entities.insert({ entityId, std::move(node) });
}

void SysSpatialImpl::removeEntity(EntityId entityId)
{
  assert(m_dfsHierarchy.size() > 0);
  if (entityId == m_dfsHierarchy[0]) {
    EXCEPTION("Can't delete root node");
  }

  auto it = m_entities.find(entityId);
  if (it == m_entities.end()) {
    return;
  }

  auto& entityNode = *it->second;
  size_t parentId = entityNode.parentId;
  size_t index = entityNode.index;
  size_t n = entityNode.numDescendents + 1; // The node plus its descendents
  size_t remainder = m_dfsHierarchy.size() - index - n;

  for (size_t i = index; i < index + n; ++i) {
    m_entities.erase(m_dfsHierarchy[i]);
  }

  std::memmove(&m_dfsHierarchy[index], &m_dfsHierarchy[index + n], remainder * sizeof(EntityId));
  std::memmove(&m_parents[index], &m_parents[index + n], remainder * sizeof(EntityId));
  m_dfsHierarchy.resize(m_dfsHierarchy.size() - n);
  m_parents.resize(m_parents.size() - n);

  auto& parentNode = *m_entities.at(parentId);
  decrementDescendentCount(parentNode, n);

  for (size_t i = index; i < m_dfsHierarchy.size(); ++i) {
    m_entities.at(m_dfsHierarchy[i])->index = i;
  }
}

bool SysSpatialImpl::hasEntity(EntityId entityId) const
{
  return m_entities.contains(entityId);
}

void SysSpatialImpl::update(Tick)
{
  Mat4x4f I = identityMatrix<float_t, 4>();
  CSpatialFlags defaultFlags{};

  Mat4x4f* parentT = &I;
  CSpatialFlags* parentFlags = &defaultFlags;
  EntityId prevParentId = NULL_ENTITY;

  // Skip the root
  for (size_t i = 1; i < m_dfsHierarchy.size(); ++i) {
    auto entityId = m_dfsHierarchy[i];
    auto parentId = m_parents[i];

    if (parentId != prevParentId) {
      parentT = &m_componentStore.component<CGlobalTransform>(parentId).transform;
      parentFlags = &m_componentStore.component<CSpatialFlags>(parentId);
      prevParentId = parentId;
    }

    auto& localT = m_componentStore.component<CLocalTransform>(entityId);
    auto& globalT = m_componentStore.component<CGlobalTransform>(entityId);
    auto& flags = m_componentStore.component<CSpatialFlags>(entityId);

    globalT.transform = *parentT * localT.transform;
    flags.active = parentFlags->active;
  }
}

SysSpatialPtr createSysSpatial(ComponentStore& componentStore)
{
  return std::make_unique<SysSpatialImpl>(componentStore);
}
