#include "entity_edit_mode/entity_edit_mode.hpp"
#include "editor_core.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;

namespace
{

const Vec4f GHOST_ENTITY_COLOUR = { 0.5f, 1.f, 0.5f, 0.5f };
const Vec4f BOUNDING_BOX_COLOUR = { 1.f, 1.f, 0.f, 0.7f };
const Vec4f AABB_COLOUR = { 0.f, 1.f, 0.f, 0.7f };

struct SuspendResumeState
{
  Vec3f cameraPosition = metresToWorldUnits(Vec3f{ 0.f, 0.f, 10.f });
  Vec3f cameraDirection = { 0.f, 0.f, -1.f };
  Mat3x3f cursorRotationScale = identityMatrix<3>();
  float cursorDistance = metresToWorldUnits(10.f);
};

enum class State
{
  None,
  BoundingBoxTool
};

// Transforms a unit cube (world units) to the box defined by min/max
Mat4x4f unitCubeToBoxTransform(const Vec3f& min, const Vec3f& max)
{
  auto size = max - min;
  auto centre = min + size * 0.5f;
  return translationMatrix4x4(centre) * scaleMatrix4x4(size);
}

class EntityEditModeImpl : public EntityEditMode
{
  public:
    EntityEditModeImpl(EditorCore& core);

    void activate() override;
    void deactivate() override;
    void saveChanges() override;
    void update() override;

    void setActivePrefab(const std::string& prefab) override;
    EntityId instantiatedPrefabId() const override;

    void renderBoundingBox(uint32_t index, bool render) override;
    void renderAabb(bool render) override;

    void selectBoundingBox(uint32_t index) override;

    void updateBoundingBox(const BoundingBox& box, uint32_t index) override;
    void addBoundingBox(const BoundingBox& box) override;

    void updateAabb(const Aabb& aabb) override;

    const Aabb& getAabb() const override;
    const BoundingBox& getBoundingBox(uint32_t index) const override;

    void applyTransform() override;
    void cancelTransform() override;

    void applyChangesToEntity() override;

    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;

  private:
    EditorCore& m_core;
    EntityId m_rootId = NULL_ENTITY_ID;
    SuspendResumeState m_suspendResumeState;
    Vec2f m_prevMousePos;

    State m_state = State::None;
    std::vector<BoundingBox> m_bboxes;
    uint32_t m_selectedBbox = 0;
    Aabb m_aabb;
    bool m_entityIsPrefab = false;
    std::string m_activePrefab;
    std::vector<XmlNodePtr> m_unusedPrefabXml;
    EntityId m_entityId = NULL_ENTITY_ID;
    std::vector<EntityId> m_renderedBboxIds;
    EntityId m_renderedAabbId = NULL_ENTITY_ID;
    EntityId m_cursorEntityId = NULL_ENTITY_ID;

    void constructRoot();
    EntityId constructBox(const Vec4f& colour);
    void updateCursorEntity();

    void updateRenderedAabb();
    void updateRenderedBoundingBox(uint32_t index);
};

EntityEditModeImpl::EntityEditModeImpl(EditorCore& core)
  : m_core(core)
{
  constructRoot();
}

void EntityEditModeImpl::constructRoot()
{
  Ecs& ecs = m_core.engine().ecs();

  m_rootId = ecs.idGen().getNewEntityId();
  ecs.componentStore().allocate<DSpatial>(m_rootId);

  auto& sysSpatial = ecs.system<SysSpatial>();
  sysSpatial.addEntity(m_rootId, DSpatial{
    .transform = identityMatrix<4>(),
    .parent = sysSpatial.root(),
    .enabled = true,
    .aabb{}
  });
}

void EntityEditModeImpl::activate()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  sysSpatial.setEnabled(m_rootId, true);

  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  camera.setPosition(m_suspendResumeState.cameraPosition);
  camera.setDirection(m_suspendResumeState.cameraDirection);

  if (m_cursorEntityId == NULL_ENTITY_ID) {
    m_core.showCursor();
  }
  else {
    m_core.hideCursor();
  }
  m_core.setCursorRotationScale(m_suspendResumeState.cursorRotationScale);
  m_core.setCursorDistance(m_suspendResumeState.cursorDistance);
}

void EntityEditModeImpl::deactivate()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  sysSpatial.setEnabled(m_rootId, false);

  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  m_suspendResumeState = {
    .cameraPosition = camera.getPosition(),
    .cameraDirection = camera.getDirection(),
    .cursorRotationScale = getRotation3x3(m_core.getCursorTransform()),
    .cursorDistance = m_core.getCursorDistance()
  };
}

void EntityEditModeImpl::updateAabb(const Aabb& aabb)
{
  m_aabb = aabb;

  if (m_renderedAabbId != NULL_ENTITY_ID) {
    updateRenderedAabb();
  }
}

void EntityEditModeImpl::addBoundingBox(const BoundingBox& box)
{
  m_bboxes.push_back(box);
  m_renderedBboxIds.push_back(NULL_ENTITY_ID);
}

void EntityEditModeImpl::updateBoundingBox(const BoundingBox& box, uint32_t index)
{
  ASSERT(index < m_bboxes.size(), "Index out of range");

  m_bboxes[index] = box;

  if (m_renderedBboxIds[index] != NULL_ENTITY_ID) {
    updateRenderedBoundingBox(index);
  }
}

const Aabb& EntityEditModeImpl::getAabb() const
{
  return m_aabb;
}

const BoundingBox& EntityEditModeImpl::getBoundingBox(uint32_t index) const
{
  return m_bboxes[index];
}

void EntityEditModeImpl::renderBoundingBox(uint32_t index, bool render)
{
  m_core.engine().logger().debug(STR("Rendering box " << index << ": "
    << (render ? "true" : "false")));

  if (render) {
    if (m_renderedBboxIds[index] == NULL_ENTITY_ID) {
      m_renderedBboxIds[index] = constructBox(BOUNDING_BOX_COLOUR);
      updateRenderedBoundingBox(index);
    }
  }
  else {
    if (m_renderedBboxIds[index] != NULL_ENTITY_ID) {
      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_renderedBboxIds[index]});
      m_renderedBboxIds[index] = NULL_ENTITY_ID;
    }
  }
}

void EntityEditModeImpl::renderAabb(bool render)
{
  if (render) {
    if (m_renderedAabbId == NULL_ENTITY_ID) {
      m_renderedAabbId = constructBox(AABB_COLOUR);
      updateRenderedAabb();
    }
  }
  else {
    if (m_renderedAabbId != NULL_ENTITY_ID) {
      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_renderedAabbId});
      m_renderedAabbId = NULL_ENTITY_ID;
    }
  }
}

void EntityEditModeImpl::updateRenderedAabb()
{
  auto& engine = m_core.engine();

  auto m = unitCubeToBoxTransform(m_aabb.min, m_aabb.max);
  engine.ecs().system<SysSpatial>().setLocalTransform(m_renderedAabbId, m);
}

void EntityEditModeImpl::updateRenderedBoundingBox(uint32_t index)
{
  auto& engine = m_core.engine();
  auto& bbox = m_bboxes[index];

  auto m = bbox.transform * unitCubeToBoxTransform(bbox.min, bbox.max);
  engine.ecs().system<SysSpatial>().setLocalTransform(m_renderedBboxIds[index], m);
}

void EntityEditModeImpl::applyTransform()
{
  switch (m_state) {
    case State::BoundingBoxTool: {
      assert(m_selectedBbox < m_bboxes.size());

      auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

      m_bboxes[m_selectedBbox].transform = m_core.getCursorTransform();
      if (m_renderedBboxIds[m_selectedBbox] != NULL_ENTITY_ID) {
        updateRenderedBoundingBox(m_selectedBbox);
      }

      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_cursorEntityId});
      m_cursorEntityId = NULL_ENTITY_ID;

      m_state = State::None;
      m_core.showCursor();

      break;
    }
    default: break;
  }
}

void EntityEditModeImpl::cancelTransform()
{
  switch (m_state) {
    case State::BoundingBoxTool: {
      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_cursorEntityId});
      m_cursorEntityId = NULL_ENTITY_ID;

      m_state = State::None;
      m_core.showCursor();
      
      break;
    }
    default: break;
  }
}

void EntityEditModeImpl::applyChangesToEntity()
{
  assert(m_entityId != NULL_ENTITY_ID);

  auto& componentStore = m_core.engine().ecs().componentStore();

  // TODO: Don't assume entity has all these components
  // Add helper methods to system classes?

  componentStore.component<CBoundingBox>(m_entityId).modelSpaceAabb = m_aabb;
  componentStore.component<CSpatialFlags>(m_entityId).flags.set(SpatialFlags::Dirty);

  auto& sysCollision = m_core.engine().ecs().system<SysCollision>();

  if (sysCollision.hasEntity(m_entityId)) {
    if (sysCollision.componentType(m_entityId) == CollisionComponentType::Aggregate) {
      auto& children = sysCollision.getAggregateChildren(m_entityId);
      for (size_t i = 0; i < children.size(); ++i) {
        componentStore.component<CCollisionBox>(children[i]).boundingBox = m_bboxes[i];
      }
    }
    else {
      assert(m_bboxes.size() == 1);
      componentStore.component<CCollisionBox>(m_entityId).boundingBox = m_bboxes[0];
    }
  }

  // TODO: If prefab, update all dependent entities in scene
}

EntityId EntityEditModeImpl::instantiatedPrefabId() const
{
  return m_entityIsPrefab ? m_entityId : NULL_ENTITY_ID;
}

void EntityEditModeImpl::setActivePrefab(const std::string& prefab)
{
  auto& engine = m_core.engine();

  // TODO: Only delete if the entity is unchanged. If the entity has applied changes, keep it, so
  // it can be restored.

  m_core.loadPrefab(prefab);

  if (m_entityId != NULL_ENTITY_ID) {
    engine.eventSystem().raiseEvent(ERequestDeletion{m_entityId});
  }

  auto prefabData = m_core.config().paths.prefabsDir->readFile(STR(prefab << ".xml"));
  auto prefabXml = parseXml(prefabData);

  m_unusedPrefabXml.clear();
  EntityMask changedFromPrefab;
  m_entityId = engine.entityFactory().constructEntity(m_rootId, *prefabXml, changedFromPrefab,
    m_unusedPrefabXml);
  m_entityIsPrefab = true;
  m_activePrefab = prefab;

  auto& sysCollision = m_core.engine().ecs().system<SysCollision>();
  auto& componentStore = m_core.engine().ecs().componentStore();
  m_aabb = componentStore.component<CBoundingBox>(m_entityId).modelSpaceAabb;

  m_bboxes.clear();

  for (auto id : m_renderedBboxIds) {
    if (id != NULL_ENTITY_ID) {
      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{id});
    }
  }
  m_renderedBboxIds.clear();

  if (m_renderedAabbId != NULL_ENTITY_ID) {
    m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_renderedAabbId});
  }

  if (sysCollision.hasEntity(m_entityId)) {
    if (sysCollision.componentType(m_entityId) == CollisionComponentType::Aggregate) {
      for (auto childId : sysCollision.getAggregateChildren(m_entityId)) {
        m_bboxes.push_back(componentStore.component<CCollisionBox>(childId).boundingBox);
        m_renderedBboxIds.push_back(NULL_ENTITY_ID);
      }
    }
    else {
      m_bboxes = { componentStore.component<CCollisionBox>(m_entityId).boundingBox };
      m_renderedBboxIds = { NULL_ENTITY_ID };
    }
  }

  assert(m_bboxes.size() == m_renderedBboxIds.size());

  m_selectedBbox = 0;
}

void EntityEditModeImpl::selectBoundingBox(uint32_t index)
{
  ASSERT(index < m_bboxes.size(), "Index out of range");

  m_selectedBbox = index;

  if (m_cursorEntityId == NULL_ENTITY_ID) {
    m_cursorEntityId = constructBox(GHOST_ENTITY_COLOUR);
  }

  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  auto& camera = sysRender3d.camera();
  auto& camDir = camera.getDirection();
  Vec3f entityPos = getTranslation(m_bboxes[m_selectedBbox].transform);

  camera.setPosition(entityPos - camDir * m_core.getCursorDistance());

  m_core.setCursorRotationScale(getRotation3x3(m_bboxes[m_selectedBbox].transform));

  m_state = State::BoundingBoxTool;
  m_core.hideCursor();
}

EntityId EntityEditModeImpl::constructBox(const Vec4f& colour)
{
  auto& ecs = m_core.engine().ecs();
  auto id = ecs.idGen().getNewEntityId();

  ecs.componentStore().allocate<DSpatial, DModel>(id);

  auto& sysSpatial = ecs.system<SysSpatial>();
  auto& sysRender3d = ecs.system<SysRender3d>();

  Vec3f sizeInMetres = worldUnitsToMetres(Vec3f{ 1.f, 1.f, 1.f });

  DSpatial spatial{};
  spatial.parent = m_rootId;
  spatial.aabb = {
    .min = metresToWorldUnits(-sizeInMetres * 0.5f),
    .max = metresToWorldUnits(sizeInMetres * 0.5f)
  };

  sysSpatial.addEntity(id, spatial);

  auto mesh = render::cuboid(sizeInMetres, { 1.f, 1.f });
  mesh->featureSet = render::MeshFeatureSet{
    .vertexLayout = {
      render::BufferUsage::AttrPosition,
      render::BufferUsage::AttrNormal,
      render::BufferUsage::AttrTexCoord
    },
    .flags{}
  };

  auto material = std::make_unique<render::Material>();
  material->colour = colour;
  material->featureSet = {
    .flags = bitflag(render::MaterialFeatures::HasTransparency)
  };

  auto& renderResourceLoader = m_core.engine().renderResourceLoader();

  auto model = std::make_unique<Model>();
  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .lods = { renderResourceLoader.loadMeshAsync(std::move(mesh)) },
      .material = renderResourceLoader.loadMaterialAsync(std::move(material)),
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  auto render = std::make_unique<DModel>();
  render->model = m_core.engine().modelLoader().loadModelAsync(std::move(model)).wait();

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

void EntityEditModeImpl::onKeyDown(KeyboardKey key)
{

}

void EntityEditModeImpl::onKeyUp(KeyboardKey key)
{
  
}

void EntityEditModeImpl::onMouseLeftBtnDown()
{

}

void EntityEditModeImpl::onMouseLeftBtnUp()
{

}

void EntityEditModeImpl::onMouseMove(float x, float y)
{
  auto& inputState = m_core.inputState();

  if (inputState.mouseButtonsPressed.contains(MouseButton::Left)) {
    float speed = 3.f;
    float dx = x - m_prevMousePos[0];
    float dy = y - m_prevMousePos[1];

    auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();
    camera.rotate(-dy * speed, dx * speed);
  }

  m_prevMousePos = { x, y };
}

void EntityEditModeImpl::saveChanges()
{
  if (m_entityIsPrefab) {
    // TODO: Save all changed prefabs

    auto xmlEntity = createXmlNode("entity");
    xmlEntity->setAttribute("type", m_activePrefab);

    for (SystemId systemId = 0; systemId < m_core.engine().ecs().numSystems(); ++systemId) {
      auto& system = m_core.engine().ecs().getSystem(systemId);
      auto node = system.componentToXml(m_entityId, NULL_ENTITY_ID);
      if (node != nullptr) {
        xmlEntity->addChild(std::move(node));
      }
    }

    for (auto& xmlUnknownSystem : m_unusedPrefabXml) {
      xmlEntity->addChild(xmlUnknownSystem->clone());
    }

    std::stringstream stream;
    xmlEntity->write(stream);

    auto xmlString = stream.str();

    m_core.config().paths.prefabsDir->writeFile(STR(m_activePrefab << ".xml"), xmlString.data(),
      xmlString.size());
  }
  else {
    // TODO
  }
}

void EntityEditModeImpl::updateCursorEntity()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  switch (m_state) {
    case State::BoundingBoxTool: {
      assert(m_selectedBbox < m_bboxes.size());

      auto t = unitCubeToBoxTransform(m_bboxes[m_selectedBbox].min, m_bboxes[m_selectedBbox].max);
      auto m = m_core.getCursorTransform() * t;
      sysSpatial.setLocalTransform(m_cursorEntityId, m);

      break;
    }
    default: break;
  }
}

void EntityEditModeImpl::update()
{
  updateCursorEntity();
}

} // namespace

EntityEditModePtr createEntityEditMode(EditorCore& core)
{
  return std::make_unique<EntityEditModeImpl>(core);
}
