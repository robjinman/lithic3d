#include "entity_edit_mode/entity_edit_mode.hpp"
#include "editor_core.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;

namespace
{

// Transforms a unit cube (1x1x1 world units) to the box defined by min/max
Mat4x4f unitCubeToBoxTransform(const Vec3f& min, const Vec3f& max)
{
  auto size = max - min;
  auto centre = min + size * 0.5f;
  return translationMatrix4x4(centre) * scaleMatrix4x4(size);
}

// Transforms a unit cylinder to the cylinder defined by radius/length
Mat4x4f unitCylinderToCylinderTransform(float radius, float length)
{
  return scaleMatrix4x4({ radius * 2.f, length, radius * 2.f });
}

// Transforms a unit sphere to the sphere defined by radius
Mat4x4f unitSphereToSphereTransform(float radius)
{
  return scaleMatrix4x4({ radius, radius, radius });
}

}

ShapePtr createBoxShape(const lithic3d::BoundingBox& box)
{
  return std::make_unique<BoxShape>(box);
}

ShapePtr createCylinderShape(const lithic3d::Cylinder& cylinder)
{
  return std::make_unique<CylinderShape>(cylinder);
}

ShapePtr createOvoidShape(const lithic3d::Ovoid& ovoid)
{
  return std::make_unique<OvoidShape>(ovoid);
}

BoxShape::BoxShape(const BoundingBox& box)
  : Shape(ShapeType::Box)
  , box(box) {}

std::unique_ptr<Shape> BoxShape::clone() const
{
  return std::make_unique<BoxShape>(box);
}

Mat4x4f BoxShape::getShapeTransform() const
{
  return unitCubeToBoxTransform(box.min, box.max);
}

const Mat4x4f& BoxShape::getTransform() const
{
  return box.transform;
}

void BoxShape::setTransform(const Mat4x4f& t)
{
  box.transform = t;
}

CylinderShape::CylinderShape(const Cylinder& cylinder)
  : Shape(ShapeType::Cylinder)
  , cylinder(cylinder) {}

std::unique_ptr<Shape> CylinderShape::clone() const
{
  return std::make_unique<CylinderShape>(cylinder);
}

Mat4x4f CylinderShape::getShapeTransform() const
{
  return unitCylinderToCylinderTransform(cylinder.radius, cylinder.height);
}

const Mat4x4f& CylinderShape::getTransform() const
{
  return cylinder.transform;
}

void CylinderShape::setTransform(const Mat4x4f& t)
{
  cylinder.transform = t;
}

OvoidShape::OvoidShape(const Ovoid& ovoid)
  : Shape(ShapeType::Ovoid)
  , ovoid(ovoid) {}

std::unique_ptr<Shape> OvoidShape::clone() const
{
  return std::make_unique<OvoidShape>(ovoid);
}

Mat4x4f OvoidShape::getShapeTransform() const
{
  return unitSphereToSphereTransform(ovoid.radius);
}

const Mat4x4f& OvoidShape::getTransform() const
{
  return ovoid.transform;
}

void OvoidShape::setTransform(const Mat4x4f& t)
{
  ovoid.transform = t;
}

namespace
{

const Vec4f GHOST_ENTITY_COLOUR = { 0.5f, 1.f, 0.5f, 0.5f };
const Vec4f BOUNDING_BOX_COLOUR = { 1.f, 0.f, 1.f, 0.7f };
const Vec4f CYLINDER_COLOUR = { 1.f, 1.f, 0.f, 0.7f };
const Vec4f OVOID_COLOUR = { 1.f, 0.7f, 0.7f, 0.7f };
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
  ShapeTransformTool
};

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

    void renderShape(uint32_t index, bool render) override;
    void renderAabb(bool render) override;

    void selectShape(uint32_t index) override;

    void updateShape(const Shape& shape, uint32_t index) override;
    void addShape(const Shape& box) override;

    void updateAabb(const Aabb& aabb) override;

    const Aabb& getAabb() const override;
    const Shape& getShape(uint32_t index) const override;

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
    std::vector<ShapePtr> m_shapes;
    uint32_t m_selectedShape = 0;
    Aabb m_aabb;
    bool m_entityIsPrefab = false;
    std::string m_activePrefab;
    std::vector<XmlNodePtr> m_unusedPrefabXml;
    EntityId m_entityId = NULL_ENTITY_ID;
    std::vector<EntityId> m_renderedShapeIds;
    EntityId m_renderedAabbId = NULL_ENTITY_ID;
    EntityId m_cursorEntityId = NULL_ENTITY_ID;

    void constructRoot();
    EntityId constructBoxEntity(const Vec4f& colour);
    EntityId constructCylinderEntity(const Vec4f& colour);
    EntityId constructOvoidEntity(const Vec4f& colour);
    EntityId constructShapeEntity(ShapeType type, bool isGhost);
    void updateCursorEntity();

    void updateRenderedAabb();
    void updateRenderedShape(uint32_t index);
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
    .cursorRotationScale = get3x3submatrix(m_core.getCursorTransform()),
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

void EntityEditModeImpl::addShape(const Shape& shape)
{
  m_shapes.push_back(shape.clone());
  m_renderedShapeIds.push_back(NULL_ENTITY_ID);
}

void EntityEditModeImpl::updateShape(const Shape& shape, uint32_t index)
{
  ASSERT(index < m_shapes.size(), "Index out of range");

  m_shapes[index] = shape.clone();

  if (m_renderedShapeIds[index] != NULL_ENTITY_ID) {
    updateRenderedShape(index);
  }
}

const Aabb& EntityEditModeImpl::getAabb() const
{
  return m_aabb;
}

const Shape& EntityEditModeImpl::getShape(uint32_t index) const
{
  return *m_shapes[index];
}

EntityId EntityEditModeImpl::constructShapeEntity(ShapeType type, bool isGhost)
{
  switch (type) {
    case ShapeType::Box:
      return constructBoxEntity(isGhost ? GHOST_ENTITY_COLOUR : BOUNDING_BOX_COLOUR);
    case ShapeType::Cylinder:
      return constructCylinderEntity(isGhost ? GHOST_ENTITY_COLOUR : CYLINDER_COLOUR);
    case ShapeType::Ovoid:
      return constructOvoidEntity(isGhost ? GHOST_ENTITY_COLOUR : OVOID_COLOUR);
  }
}

void EntityEditModeImpl::renderShape(uint32_t index, bool render)
{
  m_core.engine().logger().debug(STR("Rendering box " << index << ": "
    << (render ? "true" : "false")));

  if (render) {
    if (m_renderedShapeIds[index] == NULL_ENTITY_ID) {
      assert(index < m_shapes.size());
      auto& shape = *m_shapes[index];

      m_renderedShapeIds[index] = constructShapeEntity(shape.type, false);
      updateRenderedShape(index);
    }
  }
  else {
    if (m_renderedShapeIds[index] != NULL_ENTITY_ID) {
      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_renderedShapeIds[index]});
      m_renderedShapeIds[index] = NULL_ENTITY_ID;
    }
  }
}

void EntityEditModeImpl::renderAabb(bool render)
{
  if (render) {
    if (m_renderedAabbId == NULL_ENTITY_ID) {
      m_renderedAabbId = constructBoxEntity(AABB_COLOUR);
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

void EntityEditModeImpl::updateRenderedShape(uint32_t index)
{
  auto& engine = m_core.engine();
  auto& shape = m_shapes[index];

  Mat4x4f m = shape->getTransform() * shape->getShapeTransform();

  engine.ecs().system<SysSpatial>().setLocalTransform(m_renderedShapeIds[index], m);
}

void EntityEditModeImpl::applyTransform()
{
  switch (m_state) {
    case State::ShapeTransformTool: {
      assert(m_selectedShape < m_shapes.size());

      auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

      auto& shape = *m_shapes[m_selectedShape];
      shape.setTransform(m_core.getCursorTransform());

      if (m_renderedShapeIds[m_selectedShape] != NULL_ENTITY_ID) {
        updateRenderedShape(m_selectedShape);
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
    case State::ShapeTransformTool: {
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

  auto setShapeOnComponent = [&sysCollision, &componentStore](EntityId id, const Shape& shape) {
    auto type = sysCollision.componentType(id);
    switch (type) {
      case CollisionComponentType::StaticBox: {
        auto& box = dynamic_cast<const BoxShape&>(shape).box;
        componentStore.component<CCollisionBox>(id).boundingBox = box;
        break;
      }
      case CollisionComponentType::Cylinder: {
        auto& cylinder = dynamic_cast<const CylinderShape&>(shape).cylinder;
        componentStore.component<CCollisionCylinder>(id).cylinder = cylinder;
        break;
      }
      case CollisionComponentType::Sphere: {
        auto& ovoid = dynamic_cast<const OvoidShape&>(shape).ovoid;
        componentStore.component<CCollisionSphere>(id).ovoid = ovoid;
        break;
      }
    }
  };

  if (sysCollision.hasEntity(m_entityId)) {
    if (sysCollision.componentType(m_entityId) == CollisionComponentType::Aggregate) {
      auto& children = sysCollision.getAggregateChildren(m_entityId);
      for (size_t i = 0; i < children.size(); ++i) {
        setShapeOnComponent(children[i], *m_shapes[i]);
      }
    }
    else {
      assert(m_shapes.size() == 1);
      setShapeOnComponent(m_entityId, *m_shapes[0]);
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

  m_shapes.clear();

  for (auto id : m_renderedShapeIds) {
    if (id != NULL_ENTITY_ID) {
      m_core.engine().eventSystem().raiseEvent(ERequestDeletion{id});
    }
  }
  m_renderedShapeIds.clear();

  if (m_renderedAabbId != NULL_ENTITY_ID) {
    m_core.engine().eventSystem().raiseEvent(ERequestDeletion{m_renderedAabbId});
  }

  auto getShapeFromComponent = [&sysCollision, &componentStore](EntityId id) -> ShapePtr {
    auto type = sysCollision.componentType(id);
    switch (type) {
      case CollisionComponentType::StaticBox: {
        auto& box = componentStore.component<CCollisionBox>(id).boundingBox;
        return std::make_unique<BoxShape>(box);
      }
      case CollisionComponentType::Cylinder: {
        auto& cylinder = componentStore.component<CCollisionCylinder>(id).cylinder;
        return std::make_unique<CylinderShape>(cylinder);
      }
      case CollisionComponentType::Sphere: {
        auto& ovoid = componentStore.component<CCollisionSphere>(id).ovoid;
        return std::make_unique<OvoidShape>(ovoid);
      }
      default: {
        EXCEPTION("Not implemented");
      }
    }
  };

  if (sysCollision.hasEntity(m_entityId)) {
    if (sysCollision.componentType(m_entityId) == CollisionComponentType::Aggregate) {
      for (auto childId : sysCollision.getAggregateChildren(m_entityId)) {
        m_shapes.push_back(getShapeFromComponent(childId));
        m_renderedShapeIds.push_back(NULL_ENTITY_ID);
      }
    }
    else {
      m_shapes.push_back(getShapeFromComponent(m_entityId));
      m_renderedShapeIds = { NULL_ENTITY_ID };
    }
  }

  assert(m_shapes.size() == m_renderedShapeIds.size());

  m_selectedShape = 0;
}

void EntityEditModeImpl::selectShape(uint32_t index)
{
  ASSERT(index < m_shapes.size(), "Index out of range");

  m_selectedShape = index;

  if (m_cursorEntityId == NULL_ENTITY_ID) {
    switch(m_shapes[m_selectedShape]->type) {
      case ShapeType::Box: {
        m_cursorEntityId = constructBoxEntity(GHOST_ENTITY_COLOUR);
        break;
      }
      case ShapeType::Cylinder: {
        m_cursorEntityId = constructCylinderEntity(GHOST_ENTITY_COLOUR);
        break;
      }
      case ShapeType::Ovoid: {
        m_cursorEntityId = constructOvoidEntity(GHOST_ENTITY_COLOUR);
        break;
      }
    }
  }

  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  auto& camera = sysRender3d.camera();
  auto& camDir = camera.getDirection();

  auto& transform = m_shapes[m_selectedShape]->getTransform();
  Vec3f entityPos = getTranslation(transform);

  camera.setPosition(entityPos - camDir * m_core.getCursorDistance());

  m_core.setCursorRotationScale(get3x3submatrix(transform));

  m_state = State::ShapeTransformTool;
  m_core.hideCursor();
}

EntityId EntityEditModeImpl::constructBoxEntity(const Vec4f& colour)
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

EntityId EntityEditModeImpl::constructCylinderEntity(const Vec4f& colour)
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

  auto mesh = render::cylinder(sizeInMetres[1], 0.5f * sizeInMetres[0], true);
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

EntityId EntityEditModeImpl::constructOvoidEntity(const Vec4f& colour)
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

  auto mesh = render::sphere(sizeInMetres[1]);
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
    case State::ShapeTransformTool: {
      assert(m_selectedShape < m_shapes.size());

      auto m = m_core.getCursorTransform() * m_shapes[m_selectedShape]->getShapeTransform();
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
