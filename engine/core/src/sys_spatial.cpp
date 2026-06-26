#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/graph.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/xml_utils.hpp"
#include <unordered_map>
#include <array>
#include <cstring>

namespace lithic3d
{

Aabb transformAabb(const Aabb& aabb, const Mat4x4f& m)
{
  Vec3f d = aabb.max - aabb.min;

  Aabb bounds{
    .min{
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max()
    },
    .max{
      std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::lowest()
    }
  };

  for (uint32_t i = 0; i < 8; ++i) {
    Vec3f p = aabb.min + Vec3f{
      i & 0b001 ? d[0] : 0.f,
      i & 0b010 ? d[1] : 0.f,
      i & 0b100 ? d[2] : 0.f
    };

    auto q = (m * Vec4f{ p[0], p[1], p[2], 1.f }).sub<3>();

    for (uint32_t j = 0; j < 3; ++j) {
      if (q[j] < bounds.min[j]) {
        bounds.min[j] = q[j];
      }
      if (q[j] > bounds.max[j]) {
        bounds.max[j] = q[j];
      }
    }
  }

  return bounds;
}

XmlNodePtr toXml(const Aabb& aabb)
{
  auto xmlAabb = createXmlNode("aabb");

  auto xmlMin = createXmlNode("min");
  xmlMin->setAttribute("x", std::to_string(worldUnitsToMetres(aabb.min[0])));
  xmlMin->setAttribute("y", std::to_string(worldUnitsToMetres(aabb.min[1])));
  xmlMin->setAttribute("z", std::to_string(worldUnitsToMetres(aabb.min[2])));

  auto xmlMax = createXmlNode("max");
  xmlMax->setAttribute("x", std::to_string(worldUnitsToMetres(aabb.max[0])));
  xmlMax->setAttribute("y", std::to_string(worldUnitsToMetres(aabb.max[1])));
  xmlMax->setAttribute("z", std::to_string(worldUnitsToMetres(aabb.max[2])));

  xmlAabb->addChild(std::move(xmlMin));
  xmlAabb->addChild(std::move(xmlMax));

  return xmlAabb;
}

namespace
{

struct Node
{
  CLocalTransform* localT = nullptr;
  CGlobalTransform* globalT = nullptr;
};

using SceneGraph = Graph<EntityId, Node, NULL_ENTITY_ID>;

using SceneGraphPtr = std::unique_ptr<SceneGraph>;

class SysSpatialImpl : public SysSpatial
{
  public:
    SysSpatialImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    const std::string& name() const override;
    void extractComponentSpecs(const ComponentData& data,
      std::vector<ComponentSpec>& specs) const override;
    ComponentDataPtr constructComponentData(const XmlNode& data) const override;
    ComponentDataPtr constructComponentDataWithModifications(const ComponentData& base,
      const XmlNode& changes) const override;
    XmlNodePtr componentToXml(EntityId entityId, EntityId prefabId) const override;
    void addEntity(EntityId id, const ComponentData& data) override;
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event&) override {}

    const Mat4x4f& getLocalTransform(EntityId id) const override;
    const Mat4x4f& getGlobalTransform(EntityId id) const override;

    const Aabb& getAabb(EntityId entityId) const override;

    void translateEntitySelf(EntityId id, const Vec3f& t) override;

    void rotateEntityLocal(EntityId id, const Mat3x3f& rot) override;
    void translateEntityLocal(EntityId id, const Vec3f& t) override;
    void setLocalTransform(EntityId id, const Mat4x4f& m) override;

    EntityIdSet getIntersecting(const Frustum& frustum) const override;
    EntityIdSet getIntersecting(const Vec3f& rayStart, const Vec3f& rayEnd) const override;

    void addEntity(EntityId entityId, const DSpatial& data) override;
    EntityId root() const override;
    std::vector<EntityId> getDescendents(EntityId entityId) const override;
    void setEnabled(EntityId entityId, bool enabled) override;

    const LooseOctree& dbg_getOctree() const override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    SceneGraphPtr m_sceneGraph;
    LooseOctreePtr m_octree;
    bool m_shouldRewriteComponentPointers = false;

    ComponentDataPtr constructDSpatial(const XmlNode& xmlSpatial) const;
    void rewriteComponentPointers();
    void updateBoundingBox(EntityId entityId, const Mat4x4f& m);
};

SysSpatialImpl::SysSpatialImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_eventSystem(eventSystem)
{
  EntityId root = m_ecs.idGen().getNewEntityId();

  m_ecs.componentStore().allocate<DSpatial>(root);
  auto& localT = m_ecs.componentStore().instantiate<CLocalTransform>(root);
  auto& globalT = m_ecs.componentStore().instantiate<CGlobalTransform>(root);
  m_ecs.componentStore().instantiate<CSpatialFlags>(root);

  Node rootNode{
    .localT = &localT,
    .globalT = &globalT
  };

  m_sceneGraph = std::make_unique<SceneGraph>(root, rootNode);
  m_octree = createLooseOctree({ -1000.f, -1000.f, -1000.f }, 17000.f); // TODO
}

const LooseOctree& SysSpatialImpl::dbg_getOctree() const
{
  return *m_octree;
}

const Aabb& SysSpatialImpl::getAabb(EntityId entityId) const
{
  return m_ecs.componentStore().component<CBoundingBox>(entityId).modelSpaceAabb;
}

EntityIdSet SysSpatialImpl::getIntersecting(const Frustum& frustum) const
{
  return m_octree->getIntersecting(frustum);
}

EntityIdSet SysSpatialImpl::getIntersecting(const Vec3f& rayStart, const Vec3f& rayEnd) const
{
  return m_octree->getIntersecting(rayStart, rayEnd);
}

EntityId SysSpatialImpl::root() const
{
  return m_sceneGraph->dfs[0].key;
}

void SysSpatialImpl::rewriteComponentPointers()
{
  for (size_t i = 1; i < m_sceneGraph->dfs.size(); ++i) {
    auto& entity = m_sceneGraph->dfs[i];
    auto& localT = m_ecs.componentStore().component<CLocalTransform>(entity.key);
    auto& globalT = m_ecs.componentStore().component<CGlobalTransform>(entity.key);
    entity.value.localT = &localT;
    entity.value.globalT = &globalT;
  }

  m_shouldRewriteComponentPointers = false;
}

void SysSpatialImpl::addEntity(EntityId entityId, const DSpatial& data)
{
  auto& parentFlags = m_ecs.componentStore().component<CSpatialFlags>(data.parent).flags;
  auto& localT = m_ecs.componentStore().component<CLocalTransform>(entityId);
  auto& globalT = m_ecs.componentStore().component<CGlobalTransform>(entityId);

  Node node{
    .localT = &localT,
    .globalT = &globalT
  };

  m_sceneGraph->addItem(entityId, node, data.parent);
  m_ecs.componentStore().instantiate<CLocalTransform>(entityId).transform = data.transform;
  auto& flags = m_ecs.componentStore().instantiate<CSpatialFlags>(entityId).flags;
  flags.set(SpatialFlags::Enabled, data.enabled);
  flags.set(SpatialFlags::ParentEnabled,
    parentFlags.test(SpatialFlags::ParentEnabled) && parentFlags.test(SpatialFlags::Enabled));  

  m_ecs.componentStore().instantiate<CGlobalTransform>(entityId);
  m_ecs.componentStore().instantiate<CBoundingBox>(entityId).modelSpaceAabb = data.aabb;

  m_shouldRewriteComponentPointers = true;
}

const std::string& SysSpatialImpl::name() const
{
  static const std::string name = "spatial";
  return name;
}

void SysSpatialImpl::extractComponentSpecs(const ComponentData& data,
  std::vector<ComponentSpec>& specs) const
{
  extractSpecs<DSpatial>(data, specs);
  // ...
}

ComponentDataPtr SysSpatialImpl::constructDSpatial(const XmlNode& xmlSpatial) const
{
  auto iTransform = xmlSpatial.child("transform");
  auto iAabb = xmlSpatial.child("aabb");

  bool hasTransform = iTransform != xmlSpatial.end();
  bool hasAabb = iAabb != xmlSpatial.end();

  return std::make_unique<ComponentDataWrapper<DSpatial>>(DSpatial{
    .transform = hasTransform ? constructTransform(*iTransform) : identityMatrix<4>(),
    .parent = root(),
    .enabled = true,
    .aabb = hasAabb ? constructAabb(*iAabb) : Aabb{}
  });
}

ComponentDataPtr SysSpatialImpl::constructComponentData(const XmlNode& xmlSysSpatial) const
{
  auto& xmlComp = *xmlSysSpatial.begin();
  if (xmlComp.name() == "spatial") {
    return constructDSpatial(xmlComp);
  }
  // ...
  else {
    EXCEPTION("Bad component data type for spatial system");
  }
}

ComponentDataPtr SysSpatialImpl::constructComponentDataWithModifications(const ComponentData& base,
  const XmlNode& xmlSysSpatial) const
{
  auto& xmlComp = *xmlSysSpatial.begin();
  if (xmlComp.name() == "spatial") {
    assert(base.typeId() == typeid(DSpatial).hash_code());
    auto& baseSpatial = dynamic_cast<const ComponentDataWrapper<DSpatial>&>(base).data();

    auto iTransform = xmlComp.child("transform");
    auto iAabb = xmlComp.child("aabb");

    bool hasTransform = iTransform != xmlComp.end();
    bool hasAabb = iAabb != xmlComp.end();

    return std::make_unique<ComponentDataWrapper<DSpatial>>(DSpatial{
      .transform = hasTransform ? constructTransform(*iTransform) : baseSpatial.transform,
      .parent = baseSpatial.parent,   // TODO
      .enabled = true,
      .aabb = hasAabb ? constructAabb(*iAabb) : baseSpatial.aabb
    });
  }
  // ...
  else {
    EXCEPTION("Bad component data type for spatial system");
  }
}

XmlNodePtr SysSpatialImpl::componentToXml(EntityId entityId, EntityId prefabId) const
{
  if (!hasEntity(entityId)) {
    return nullptr;
  }

  auto xmlSysSpatial = createXmlNode("spatial");
  auto xmlSpatial = createXmlNode("spatial");

  auto localTransform = getLocalTransform(entityId);
  auto aabb = getAabb(entityId);

  bool writeTransform = true;
  bool writeAabb = true;

  if (prefabId != NULL_ENTITY_ID) {
    writeTransform = getLocalTransform(prefabId) != localTransform;

    auto prefabAabb = getAabb(prefabId);
    writeAabb = !(prefabAabb.min == aabb.min && prefabAabb.max == aabb.max);
  }

  if (writeTransform) {
    xmlSpatial->addChild(toXml(localTransform));
  }
  if (writeAabb) {
    xmlSpatial->addChild(toXml(aabb));
  }

  xmlSysSpatial->addChild(std::move(xmlSpatial));

  return xmlSysSpatial;
}

void SysSpatialImpl::addEntity(EntityId id, const ComponentData& data)
{
  if (data.typeId() == typeid(DSpatial).hash_code()) {
    auto& spatial = dynamic_cast<const ComponentDataWrapper<DSpatial>&>(data).data();
    addEntity(id, spatial);
  }
  // ...
  else {
    EXCEPTION("Bad component data type for spatial system");
  }
}

std::vector<EntityId> SysSpatialImpl::getDescendents(EntityId entityId) const
{
  auto& node = *m_sceneGraph->nodes.at(entityId);
  auto n = node.numDescendents;
  auto idx = node.index;

  std::vector<EntityId> descendents;

  for (size_t i = 0; i < n; ++i) {
    descendents.push_back(m_sceneGraph->dfs[idx + 1 + i].key);
  }

  return descendents;
}

void SysSpatialImpl::removeEntity(EntityId entityId)
{
  if (!hasEntity(entityId)) {
    return;
  }

  auto descendents = getDescendents(entityId);

  for (auto id : descendents) {
    m_octree->remove(id);
  }
  m_octree->remove(entityId);

  // Also removes all descendents
  m_sceneGraph->removeItem(entityId);

  m_shouldRewriteComponentPointers = true;
}

bool SysSpatialImpl::hasEntity(EntityId entityId) const
{
  return m_sceneGraph->hasItem(entityId);
}

const Mat4x4f& SysSpatialImpl::getGlobalTransform(EntityId id) const
{
  auto& componentStore = m_ecs.componentStore();
#ifndef NDEBUG
  if (componentStore.component<CSpatialFlags>(id).flags.test(SpatialFlags::Dirty)) {
    m_logger.debug(STR("Warning: Entity " << id << " global transform not at latest"));
  }
#endif
  return componentStore.component<CGlobalTransform>(id).transform;
}

const Mat4x4f& SysSpatialImpl::getLocalTransform(EntityId id) const
{
  auto& componentStore = m_ecs.componentStore();
  return componentStore.component<CLocalTransform>(id).transform;
}

void SysSpatialImpl::translateEntitySelf(EntityId id, const Vec3f& t)
{
  auto& componentStore = m_ecs.componentStore();
  auto& localT = componentStore.component<CLocalTransform>(id);
  auto& flags = m_ecs.componentStore().component<CSpatialFlags>(id);
  localT.transform = localT.transform * translationMatrix4x4(t);
  flags.flags.set(SpatialFlags::Dirty);
}

void SysSpatialImpl::rotateEntityLocal(EntityId id, const Mat3x3f& rot)
{
  auto& componentStore = m_ecs.componentStore();
  auto& localT = componentStore.component<CLocalTransform>(id);
  auto& flags = m_ecs.componentStore().component<CSpatialFlags>(id);
  localT.transform = applyRotation(localT.transform, rot);
  flags.flags.set(SpatialFlags::Dirty);
}

void SysSpatialImpl::translateEntityLocal(EntityId id, const Vec3f& t)
{
  auto& componentStore = m_ecs.componentStore();
  auto& localT = componentStore.component<CLocalTransform>(id);
  auto& flags = m_ecs.componentStore().component<CSpatialFlags>(id);
  localT.transform = translationMatrix4x4(t) * localT.transform;
  flags.flags.set(SpatialFlags::Dirty);
}

void SysSpatialImpl::setLocalTransform(EntityId id, const Mat4x4f& m)
{
  auto& localT = m_ecs.componentStore().component<CLocalTransform>(id);
  auto& flags = m_ecs.componentStore().component<CSpatialFlags>(id);
  localT.transform = m;
  flags.flags.set(SpatialFlags::Dirty);
}

void SysSpatialImpl::updateBoundingBox(EntityId entityId, const Mat4x4f& m)
{
  auto& box = m_ecs.componentStore().component<CBoundingBox>(entityId);

  box.worldSpaceAabb = transformAabb(box.modelSpaceAabb, m);

  Vec3f pos = box.worldSpaceAabb.min + (box.worldSpaceAabb.max - box.worldSpaceAabb.min) * 0.5f;
  float radius = (box.worldSpaceAabb.max - box.worldSpaceAabb.min).magnitude() * 0.5f;

  m_octree->move(entityId, pos, radius);
}

void SysSpatialImpl::update(Tick, const InputState&)
{
  if (m_shouldRewriteComponentPointers) {
    rewriteComponentPointers();
  }

  Mat4x4f I = identityMatrix<4>();

  Mat4x4f* parentT = &I;
  EntityId prevParentId = NULL_ENTITY_ID;

  // Skip the root
  for (size_t i = 1; i < m_sceneGraph->dfs.size(); ++i) {
    auto& entity = m_sceneGraph->dfs[i];
    auto parentId = m_sceneGraph->parents[i];

    if (parentId != prevParentId) {
      parentT = &m_ecs.componentStore().component<CGlobalTransform>(parentId).transform;
      prevParentId = parentId;
    }

    // TODO: Skip if entity is static or local transform is non-dirty

    entity.value.globalT->transform = *parentT * entity.value.localT->transform;

    updateBoundingBox(entity.key, entity.value.globalT->transform);
  }
}

void SysSpatialImpl::setEnabled(EntityId entityId, bool enabled)
{
  auto& node = *m_sceneGraph->nodes.at(entityId);
  auto& flags = m_ecs.componentStore().component<CSpatialFlags>(entityId).flags;

  if (flags.test(SpatialFlags::Enabled) == enabled) {
    return;
  }

  flags.set(SpatialFlags::Enabled, enabled);

  EntityId prevParentId = entityId;
  bool parentEnabled = flags.test(SpatialFlags::ParentEnabled)
    && flags.test(SpatialFlags::Enabled); // TODO: Why?

  // Propagate flags to descendents
  for (size_t i = node.index + 1; i <= node.index + node.numDescendents; ++i) {
    auto entityId = m_sceneGraph->dfs[i].key;
    auto parentId = m_sceneGraph->parents[i];

    if (parentId != prevParentId) {
      auto& parentFlags = m_ecs.componentStore().component<CSpatialFlags>(parentId).flags;
      parentEnabled = parentFlags.test(SpatialFlags::ParentEnabled) &&
        parentFlags.test(SpatialFlags::Enabled);
      prevParentId = parentId;
    }

    auto& descendentFlags = m_ecs.componentStore().component<CSpatialFlags>(entityId).flags;
    descendentFlags.set(SpatialFlags::ParentEnabled, parentEnabled);
  }

  if (enabled) {
    m_eventSystem.raiseEvent(EEntityEnable{entityId, EntityIdSet{ entityId }});
  }
}

} // namespace

SysSpatialPtr createSysSpatial(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysSpatialImpl>(ecs, eventSystem, logger);
}

} // namespace lithic3d
