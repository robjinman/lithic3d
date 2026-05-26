#include "scene_edit_mode/scene_edit_mode.hpp"
#include "editor_core.hpp"
#include "prefabs_panel.hpp"
#include "scene_panel.hpp"
#include "current_transform_panel.hpp"
#include "components_panel.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;
namespace fs = std::filesystem;

namespace
{

const Vec4f GhostEntityColour = { 0.5f, 1.f, 0.5f, 0.5f };
const Vec4f SelectedEntityColour = { 3.f, 2.f, 2.f, 1.f };

struct SliceState
{
  SliceState()
    : handle{}
    , entities{}
    , dirty(false)
  {}

  SliceState(ResourceHandle handle, std::vector<EntityInfo>&& entities, bool dirty)
    : handle(handle)
    , entities(std::move(entities))
    , dirty(dirty)
  {}

  SliceState(const SliceState&) = delete;
  SliceState(SliceState&&) = default;

  SliceState& operator=(SliceState&& rhs)
  {
    handle = rhs.handle;
    entities = std::move(rhs.entities);
    dirty = rhs.dirty;

    return *this;
  }

  ResourceHandle handle;
  std::vector<EntityInfo> entities;
  bool dirty = false;
};

struct WorldState
{
  std::map<Vec3i, SliceState> slices;
};

enum class State
{
  None,
  PrefabSelected,
  EntitySelected
};

using Callback = std::function<void()>;

struct SuspendResumeState
{
  Vec3f cameraPosition;
  Vec3f cameraDirection;
  Mat3x3f cursorRotationScale;
  float cursorDistance;
};

class SceneEditModeImpl : public SceneEditMode
{
  public:
    explicit SceneEditModeImpl(EditorCore& core);

    void activate() override;
    void deactivate() override;

    std::vector<EntityIdAndType> getEntities() const override;
    void setActivePrefab(const std::string& name) override;
    const std::string& getActivePrefab() const override;
    void instantiateActivePrefab() override;
    void cancelActivePrefab() override;

    void selectEntity(EntityId id, const std::string& type) override;
    EntityId selectedEntity() const override;
    void applyTransform() override;
    void cancelTransform() override;

    void update() override;

    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;

    EventHandle listen(Event event, const EventHandler& handler) override;

    void saveChanges() override;

  private:
    void processKeyboardInput();
    void positionCursorEntity();
    Vec2i cellFromPosition(const Vec3f& pos) const;
    void saveChangesToSlice(const Vec3f& index, const SliceState& slice);
    void saveChangesToSlice(const fs::path& cellDirName, const std::string& sliceFileName,
      const SliceState& slice);
    void loadCurrentCell();

    EditorCore& m_core;
    State m_state = State::None;
    std::map<std::string, ResourceHandle> m_prefabs;
    EventEmitterPtr m_eventEmitter;
    Vec2f m_prevMousePos;
    EntityId m_cursorEntity = NULL_ENTITY_ID;
    std::string m_cursorEntityType;
    EntityId m_selectedEntity = NULL_ENTITY_ID;
    WorldState m_worldState;

    SuspendResumeState m_suspendResumeState;
};

SceneEditModeImpl::SceneEditModeImpl(EditorCore& core)
  : m_core(core)
{
  m_eventEmitter = createEventEmitter();

  auto initialPos = metresToWorldUnits(Vec3f{ 11305.0, 30.0, 9199.0 });
  m_core.engine().ecs().system<SysRender3d>().camera().setPosition(initialPos);

  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  m_suspendResumeState = {
    .cameraPosition = camera.getPosition(),
    .cameraDirection = camera.getDirection(),
    .cursorRotationScale = getRotation3x3(m_core.getCursorTransform()),
    .cursorDistance = m_core.getCursorDistance()
  };

  loadCurrentCell();
}

void SceneEditModeImpl::activate()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  sysSpatial.setEnabled(m_core.engine().worldLoader().root(), true);

  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  camera.setPosition(m_suspendResumeState.cameraPosition);
  camera.setDirection(m_suspendResumeState.cameraDirection);

  m_core.setCursorRotationScale(m_suspendResumeState.cursorRotationScale);
  m_core.setCursorDistance(m_suspendResumeState.cursorDistance);
}

void SceneEditModeImpl::deactivate()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  sysSpatial.setEnabled(m_core.engine().worldLoader().root(), false);

  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  m_suspendResumeState = {
    .cameraPosition = camera.getPosition(),
    .cameraDirection = camera.getDirection(),
    .cursorRotationScale = getRotation3x3(m_core.getCursorTransform()),
    .cursorDistance = m_core.getCursorDistance()
  };
}

EventHandle SceneEditModeImpl::listen(Event event, const EventHandler& handler)
{
  return m_eventEmitter->subscribe(static_cast<EventId>(event), handler);
}

Vec2i SceneEditModeImpl::cellFromPosition(const Vec3f& pos) const
{
  float cellW = m_core.engine().worldLoader().worldInfo().cellWidth;
  float cellH = m_core.engine().worldLoader().worldInfo().cellHeight;

  return {
    static_cast<int>(pos[0] / cellW),
    static_cast<int>(pos[2] / cellH)
  };
}

std::vector<EntityIdAndType> SceneEditModeImpl::getEntities() const
{
  std::vector<EntityIdAndType> entities;

  for (auto& i : m_worldState.slices) {
    auto& slice = i.second;

    for (auto& entity : slice.entities) {
      entities.push_back({ entity.id, entity.type });
    }
  }

  return entities;
}

void SceneEditModeImpl::onKeyDown(KeyboardKey key)
{

}

void SceneEditModeImpl::onKeyUp(KeyboardKey key)
{
  
}

void SceneEditModeImpl::onMouseLeftBtnDown()
{

}

void SceneEditModeImpl::onMouseLeftBtnUp()
{

}

void SceneEditModeImpl::onMouseMove(float x, float y)
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

const std::string& SceneEditModeImpl::getActivePrefab() const
{
  return m_cursorEntityType;
}

void SceneEditModeImpl::setActivePrefab(const std::string& name)
{
  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();

  switch (m_state) {
    case State::EntitySelected: {
      if (m_selectedEntity != NULL_ENTITY_ID) {
        sysRender3d.setEntityColour(m_selectedEntity, { 1.f, 1.f, 1.f, 1.f });
        m_selectedEntity = NULL_ENTITY_ID;
      }
      if (m_cursorEntity != NULL_ENTITY_ID) {
        m_core.engine().ecs().removeEntity(m_cursorEntity);
        m_cursorEntity = NULL_ENTITY_ID;
      }
      break;
    }
    case State::PrefabSelected: {
      cancelActivePrefab();
      break;
    }
    case State::None: break;
  }

  m_state = State::PrefabSelected;

  auto& factory = m_core.engine().entityFactory();
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  ResourceHandle handle;
  auto i = m_prefabs.find(name);
  if (i == m_prefabs.end()) {
    handle = factory.loadPrefabAsync(name).wait();
    m_prefabs.insert(std::make_pair(name, handle));
  }
  else {
    handle = i->second;
  }

  auto transform = m_core.getCursorTransform();
  m_cursorEntity = factory.constructGhostEntity(m_core.engine().worldGrid().root(), name, transform,
    GhostEntityColour);
  m_cursorEntityType = name;
}

void SceneEditModeImpl::instantiateActivePrefab()
{
  ASSERT(m_state == State::PrefabSelected, "Bad state transition");
  cancelActivePrefab();

  auto& factory = m_core.engine().entityFactory();
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  auto transform = m_core.getCursorTransform();
  auto pos = getTranslation(transform);
  auto cell = cellFromPosition(pos);

  int sliceIdx = 1; // TODO
  auto& slice = m_worldState.slices[{ cell[0], cell[1], sliceIdx }];
  assert(slice.handle.id() != NULL_RESOURCE_ID);
  slice.entities.push_back(EntityInfo{
    factory.constructEntity(m_core.engine().worldGrid().root(), m_cursorEntityType, transform),
    m_cursorEntityType, {}});
  slice.dirty = true;

  m_eventEmitter->raise(static_cast<EventId>(Event::AddOrRemoveEntity));
}

void SceneEditModeImpl::selectEntity(EntityId id, const std::string& type)
{
  auto& factory = m_core.engine().entityFactory();
  if (!factory.hasEntityType(type)) {
    return;
  }

  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();

  switch (m_state) {
    case State::EntitySelected: {
      if (m_cursorEntity != NULL_ENTITY_ID) {
        m_core.engine().ecs().removeEntity(m_cursorEntity);
        m_cursorEntity = NULL_ENTITY_ID;
      }
      if (m_selectedEntity != NULL_ENTITY_ID) {
        sysRender3d.setEntityColour(m_selectedEntity, { 1.f, 1.f, 1.f, 1.f });
      }
      break;
    }
    case State::PrefabSelected: {
      cancelActivePrefab();
      break;
    }
    case State::None: break;
  }

  m_state = State::EntitySelected;

  auto& camera = sysRender3d.camera();
  auto& camDir = camera.getDirection();
  auto& entityTransform = m_core.engine().ecs().system<SysSpatial>().getGlobalTransform(id);
  Vec3f entityPos = getTranslation(entityTransform);

  camera.setPosition(entityPos - camDir * m_core.getCursorDistance());

  m_core.setCursorRotationScale(getRotation3x3(entityTransform));

  m_selectedEntity = id;

  sysRender3d.setEntityColour(m_selectedEntity, SelectedEntityColour);

  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  m_cursorEntity = factory.constructGhostEntity(m_core.engine().worldGrid().root(), type,
    entityTransform, GhostEntityColour);
  m_cursorEntityType = type;

  m_eventEmitter->raise(static_cast<EventId>(Event::EntitySelect));
}

EntityId SceneEditModeImpl::selectedEntity() const
{
  return m_cursorEntity;
}

void SceneEditModeImpl::applyTransform()
{
  if (m_state != State::EntitySelected) {
    return;
  }

  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  // TODO: Entity hierarchies?
  auto& transform = sysSpatial.getGlobalTransform(m_cursorEntity);
  sysSpatial.setLocalTransform(m_selectedEntity, transform);

  Vec3f pos = getTranslation(transform);
  auto cell = cellFromPosition(pos);

  int sliceIdx = 1; // TODO
  auto& slice = m_worldState.slices[{ cell[0], cell[1], sliceIdx }];
  slice.dirty = true;
}

void SceneEditModeImpl::cancelTransform()
{
  if (m_state != State::EntitySelected) {
    return;
  }

  m_state = State::None;

  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();

  if (m_selectedEntity != NULL_ENTITY_ID) {
    sysRender3d.setEntityColour(m_selectedEntity, { 1.f, 1.f, 1.f, 1.f });
    m_selectedEntity = NULL_ENTITY_ID;
    m_eventEmitter->raise(static_cast<EventId>(Event::EntitySelect));
  }
  if (m_cursorEntity != NULL_ENTITY_ID) {
    m_core.engine().ecs().removeEntity(m_cursorEntity);
    m_cursorEntity = NULL_ENTITY_ID;
  }
}

void SceneEditModeImpl::saveChanges()
{
  for (auto& [ index, slice ] : m_worldState.slices) {
    if (slice.dirty) {
      saveChangesToSlice(index, slice);
      slice.dirty = false;
    }
  }
}

void SceneEditModeImpl::saveChangesToSlice(const fs::path& cellDirName,
  const std::string& sliceFileName, const SliceState& slice)
{
  auto cellDir = m_core.config().paths.worldsDir->subdirectory(fs::path{"world"} / cellDirName);

  ASSERT(cellDir->fileExists(sliceFileName), "File " << sliceFileName << " doesn't exist");

  auto xmlCellSlice = createXmlNode("cell-slice");
  auto xmlEntities = createXmlNode("entities");
  for (auto& entity : slice.entities) {
    auto xmlEntity = createXmlNode("entity");
    xmlEntity->setAttribute("type", entity.type);
    if (isHashedString(entity.id)) {
      xmlEntity->setAttribute("id", getHashedString(entity.id));
    }

    for (SystemId systemId = 0; systemId < m_core.engine().ecs().numSystems(); ++systemId) {
      auto& system = m_core.engine().ecs().getSystem(systemId);
      auto node = system.componentToXml(entity.id);
      if (node != nullptr) {
        xmlEntity->addChild(std::move(node));
      }
    }

    for (auto& xmlUnknownSystem : entity.unused) {
      xmlEntity->addChild(xmlUnknownSystem->clone());
    }

    xmlEntities->addChild(std::move(xmlEntity));
  }
  xmlCellSlice->addChild(std::move(xmlEntities));

  std::stringstream stream;
  xmlCellSlice->write(stream);

  auto xmlString = stream.str();
  cellDir->writeFile(sliceFileName, xmlString.data(), xmlString.size());
}

void SceneEditModeImpl::saveChangesToSlice(const Vec3f& index, const SliceState& slice)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << index[0]
    << std::setw(3) << std::setfill('0') << index[1];
  auto cellDirName = ss.str();
  ss.str("");
  ss << std::setw(3) << std::setfill('0') << index[2] << ".xml";
  auto sliceFileName = ss.str();

  saveChangesToSlice(cellDirName, sliceFileName, slice);
}

void SceneEditModeImpl::cancelActivePrefab()
{
  ASSERT(m_state == State::PrefabSelected, "Bad state transition");
  m_state = State::None;

  if (m_cursorEntity != NULL_ENTITY_ID) {
    m_core.engine().ecs().removeEntity(m_cursorEntity);
  }
  m_cursorEntity = NULL_ENTITY_ID;
}

void SceneEditModeImpl::loadCurrentCell()
{
  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  auto cell = cellFromPosition(camera.getPosition());
  for (int i = 0; i < 6; ++i) {
    auto slice = m_core.engine().worldLoader().loadCellSliceAsync(cell[0], cell[1], i).wait();
    m_worldState.slices.insert_or_assign({ cell[0], cell[1], i }, SliceState{slice,
      m_core.engine().worldLoader().createEntities(slice.id()), false});
  }
}

void SceneEditModeImpl::processKeyboardInput()
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

void SceneEditModeImpl::positionCursorEntity()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  if (m_cursorEntity != NULL_ENTITY_ID) {
    sysSpatial.setLocalTransform(m_cursorEntity, m_core.getCursorTransform());
  }
}

void SceneEditModeImpl::update()
{
  processKeyboardInput();
  positionCursorEntity();
}

} // namespace

SceneEditModePtr createSceneEditMode(EditorCore& core)
{
  return std::make_unique<SceneEditModeImpl>(core);
}
