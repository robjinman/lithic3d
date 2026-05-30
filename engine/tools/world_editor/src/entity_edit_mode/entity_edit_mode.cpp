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

    void renderBoundingBox(bool render) override;
    void renderAabb(bool render) override;

    void selectBoundingBox() override;

    void updateBoundingBox(const BoundingBox& box) override;
    void updateAabb(const Aabb& aabb) override;

    const Aabb& getAabb() const override;
    const BoundingBox& getBoundingBox() const override;

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
    BoundingBox m_bbox;
    Aabb m_aabb;
    bool m_entityIsPrefab = false;
    std::string m_activePrefab;
    std::vector<XmlNodePtr> m_unusedPrefabXml;
    EntityId m_entityId = NULL_ENTITY_ID;
    EntityId m_renderedBboxId = NULL_ENTITY_ID;
    EntityId m_renderedAabbId = NULL_ENTITY_ID;
    EntityId m_cursorEntityId = NULL_ENTITY_ID;

    void constructRoot();
    void processKeyboardInput();
    EntityId constructBox(const Vec4f& colour);
    void updateCursorEntity();

    void updateRenderedAabb();
    void updateRenderedBoundingBox();
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

void EntityEditModeImpl::updateBoundingBox(const BoundingBox& box)
{
  m_bbox = box;

  if (m_renderedBboxId != NULL_ENTITY_ID) {
    updateRenderedBoundingBox();
  }
}

const Aabb& EntityEditModeImpl::getAabb() const
{
  return m_aabb;
}

const BoundingBox& EntityEditModeImpl::getBoundingBox() const
{
  return m_bbox;
}

void EntityEditModeImpl::renderBoundingBox(bool render)
{
  if (render) {
    if (m_renderedBboxId == NULL_ENTITY_ID) {
      m_renderedBboxId = constructBox(BOUNDING_BOX_COLOUR);
      updateRenderedBoundingBox();
    }
  }
  else {
    if (m_renderedBboxId != NULL_ENTITY_ID) {
      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_renderedBboxId});
      m_renderedBboxId = NULL_ENTITY_ID;
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

void EntityEditModeImpl::updateRenderedBoundingBox()
{
  auto& engine = m_core.engine();

  auto m = m_bbox.transform * unitCubeToBoxTransform(m_bbox.min, m_bbox.max);
  engine.ecs().system<SysSpatial>().setLocalTransform(m_renderedBboxId, m);
}

void EntityEditModeImpl::applyTransform()
{
  switch (m_state) {
    case State::BoundingBoxTool: {
      auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

      m_bbox.transform = m_core.getCursorTransform();
      if (m_renderedBboxId != NULL_ENTITY_ID) {
        updateRenderedBoundingBox();
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
  componentStore.component<CCollisionBox>(m_entityId).boundingBox = m_bbox;

  // TODO: If prefab, update all dependent entities in scene
}

EntityId EntityEditModeImpl::instantiatedPrefabId() const
{
  return m_entityId;
}

void EntityEditModeImpl::setActivePrefab(const std::string& prefab)
{
  auto& engine = m_core.engine();

  // TODO: Only delete if the entity is unchanged. If the entity has applied changes, keep it, so
  // it can be restored.

  if (m_entityId != NULL_ENTITY_ID) {
    engine.eventSystem().raiseEvent(ERequestDeletion{m_entityId});
  }

  auto prefabData = m_core.config().paths.prefabsDir->readFile(STR(prefab << ".xml"));
  auto prefabXml = parseXml(prefabData);

  m_entityId = engine.entityFactory().constructEntity(m_rootId, *prefabXml, m_unusedPrefabXml);
  m_entityIsPrefab = true;
  m_activePrefab = prefab;
}

void EntityEditModeImpl::selectBoundingBox()
{
  if (m_cursorEntityId == NULL_ENTITY_ID) {
    m_cursorEntityId = constructBox(GHOST_ENTITY_COLOUR);
  }

  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  auto& camera = sysRender3d.camera();
  auto& camDir = camera.getDirection();
  Vec3f entityPos = getTranslation(m_bbox.transform);

  camera.setPosition(entityPos - camDir * m_core.getCursorDistance());

  m_core.setCursorRotationScale(getRotation3x3(m_bbox.transform));

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
      .mesh = renderResourceLoader.loadMeshAsync(std::move(mesh)),
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

// TODO: Move to core
void EntityEditModeImpl::processKeyboardInput()
{
  auto& inputState = m_core.inputState();
  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  float metresPerSecond = 6.f;
  float speed = metresToWorldUnits(metresPerSecond) / TICKS_PER_SECOND;
  const auto forward = camera.getDirection();
  const auto right = forward.cross(Vec3f{0, 1, 0});
  Vec3f direction;

  if (inputState.keysPressed.contains(KeyboardKey::W)) {
    direction += forward;
  }
  if (inputState.keysPressed.contains(KeyboardKey::S)) {
    direction -= forward;
  }
  if (inputState.keysPressed.contains(KeyboardKey::D)) {
    direction += right;
  }
  if (inputState.keysPressed.contains(KeyboardKey::A)) {
    direction -= right;
  }
  if (inputState.keysPressed.contains(KeyboardKey::Q)) {
    direction += { 0, -1, 0 };
  }
  if (inputState.keysPressed.contains(KeyboardKey::E)) {
    direction += { 0, 1, 0 };
  }

  if (direction != Vec3f{}) {
    direction = direction.normalise();
    auto delta = direction * speed;

    camera.translate(delta);
  }
}

void EntityEditModeImpl::saveChanges()
{
  if (m_entityIsPrefab) {
    // TODO: Save all changed prefabs

    auto xmlEntity = createXmlNode("entity");
    xmlEntity->setAttribute("type", m_activePrefab);

    for (SystemId systemId = 0; systemId < m_core.engine().ecs().numSystems(); ++systemId) {
      auto& system = m_core.engine().ecs().getSystem(systemId);
      auto node = system.componentToXml(m_entityId);
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

    m_core.config().paths.prefabsDir->writeFile(STR(m_activePrefab << "_test.xml"), xmlString.data(),
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

      auto m = m_core.getCursorTransform() * unitCubeToBoxTransform(m_bbox.min, m_bbox.max);
      sysSpatial.setLocalTransform(m_cursorEntityId, m);
      break;
    }
    default: break;
  }
}

void EntityEditModeImpl::update()
{
  processKeyboardInput();
  updateCursorEntity();
}

} // namespace

EntityEditModePtr createEntityEditMode(EditorCore& core)
{
  return std::make_unique<EntityEditModeImpl>(core);
}
