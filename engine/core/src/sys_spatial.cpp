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

XmlNodePtr toXml(const Mat4x4f& m)
{
  auto xmlTransform = createXmlNode("transform");
  auto xmlMatrix = createXmlNode("matrix");

  std::stringstream ss;

  for (size_t r = 0; r < 3; ++r) {
    for (size_t c = 0; c < 4; ++c) {
      ss << m.at(r, c);
      if (!(r == 2 && c == 3)) {
        ss << ",";
      }
    }
  }

  xmlMatrix->setValue(ss.str());

  xmlTransform->addChild(std::move(xmlMatrix));

  return xmlTransform;
}

namespace
{

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
    XmlNodePtr componentToXml(EntityId entityId) const override;
    void addEntity(EntityId id, const ComponentData& data) override;
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event&) override {}

    const Mat4x4f& getLocalTransform(EntityId id) const override;
    const Mat4x4f& getGlobalTransform(EntityId id) const override;

    void translateEntitySelf(EntityId id, const Vec3f& t) override;

    void rotateEntityLocal(EntityId id, const Mat3x3f& rot) override;
    void translateEntityLocal(EntityId id, const Vec3f& t) override;
    void setLocalTransform(EntityId id, const Mat4x4f& m) override;

    EntityIdSet getIntersecting(const Frustum& frustum) const override;
    EntityIdSet getIntersecting(const Vec3f& rayStart, const Vec3f& rayEnd) const override;

    void addEntity(EntityId entityId, const DSpatial& data) override;
    EntityId root() const override;
    void setEnabled(EntityId entityId, bool enabled) override;

    const LooseOctree& dbg_getOctree() const override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    GraphPtr<EntityId, NULL_ENTITY_ID> m_sceneGraph;
    LooseOctreePtr m_octree;

    ComponentDataPtr constructDSpatial(const XmlNode& xmlSpatial) const;
    void updateBoundingBox(EntityId entityId, const Mat4x4f& m);
};

SysSpatialImpl::SysSpatialImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_eventSystem(eventSystem)
{
  EntityId root = m_ecs.idGen().getNewEntityId();

  m_ecs.componentStore().allocate<DSpatial>(root);
  m_ecs.componentStore().instantiate<CLocalTransform>(root);
  m_ecs.componentStore().instantiate<CGlobalTransform>(root);
  m_ecs.componentStore().instantiate<CSpatialFlags>(root);

  m_sceneGraph = std::make_unique<Graph<EntityId, NULL_ENTITY_ID>>(root);
  m_octree = createLooseOctree({ -1000.f, -1000.f, -1000.f }, 17000.f); // TODO
}

const LooseOctree& SysSpatialImpl::dbg_getOctree() const
{
  return *m_octree;
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
  return m_sceneGraph->dfs[0];
}

void SysSpatialImpl::addEntity(EntityId entityId, const DSpatial& data)
{
  auto& parentFlags = m_ecs.componentStore().component<CSpatialFlags>(data.parent).flags;

  m_sceneGraph->addItem(entityId, data.parent);
  m_ecs.componentStore().instantiate<CLocalTransform>(entityId).transform = data.transform;
  auto& flags = m_ecs.componentStore().instantiate<CSpatialFlags>(entityId).flags;
  flags.set(SpatialFlags::Enabled, data.enabled);
  flags.set(SpatialFlags::ParentEnabled,
    parentFlags.test(SpatialFlags::ParentEnabled) && parentFlags.test(SpatialFlags::Enabled));  

  m_ecs.componentStore().instantiate<CGlobalTransform>(entityId);
  m_ecs.componentStore().instantiate<CBoundingBox>(entityId).modelSpaceAabb = data.aabb;
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

  return std::make_unique<ComponentDataWrapper<DSpatial>>(DSpatial{
    .transform = iTransform == xmlSpatial.end() ?
      identityMatrix<4>() : constructTransform(*iTransform),
    .parent = root(),
    .enabled = true,
    .aabb = iAabb == xmlSpatial.end() ? Aabb{} : constructAabb(*iAabb)
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

    return std::make_unique<ComponentDataWrapper<DSpatial>>(DSpatial{
      .transform = iTransform == xmlComp.end() ?
        baseSpatial.transform : constructTransform(*iTransform),
      .parent = baseSpatial.parent,   // TODO
      .enabled = baseSpatial.enabled, // TODO
      .aabb = baseSpatial.aabb        // TODO
    });
  }
  // ...
  else {
    EXCEPTION("Bad component data type for spatial system");
  }
}

XmlNodePtr SysSpatialImpl::componentToXml(EntityId entityId) const
{
  if (!hasEntity(entityId)) {
    return nullptr;
  }

  auto xmlSysSpatial = createXmlNode("spatial");
  auto xmlSpatial = createXmlNode("spatial");

  xmlSpatial->addChild(toXml(getLocalTransform(entityId)));
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

void SysSpatialImpl::removeEntity(EntityId entityId)
{
  m_sceneGraph->removeItem(entityId);
  m_octree->remove(entityId);
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
    auto entityId = m_sceneGraph->dfs[i];
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
