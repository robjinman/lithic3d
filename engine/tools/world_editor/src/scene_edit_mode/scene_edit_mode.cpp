#include "scene_edit_mode/scene_edit_mode.hpp"
#include "editor_core.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;
namespace fs = std::filesystem;

namespace
{

const Vec4f GHOST_ENTITY_COLOUR = { 0.5f, 1.f, 0.5f, 0.5f };
const Vec4f SELECTED_ENTITY_COLOUR = { 3.f, 2.f, 2.f, 1.f };

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

    void selectEntity(EntityId id) override;
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
    void saveChangesToSlice(const Vec3f& index, const SliceState& slice,
      std::map<std::string, EntityId>&);
    void saveChangesToSlice(const fs::path& cellDirName, const std::string& sliceFileName,
      const SliceState& slice, std::map<std::string, EntityId>&);
    void loadCurrentCell();
    void instantiateActivePrefab();
    void cancelActivePrefab();
    void applyCurrentTransformToEntity();
    void cancelCurrentEntityTransform();
    void setEntityColour(EntityId id, const Vec4f& colour);

    EditorCore& m_core;
    State m_state = State::None;
    EventEmitterPtr m_eventEmitter;
    Vec2f m_prevMousePos;
    EntityId m_cursorEntityId = NULL_ENTITY_ID;
    std::string m_cursorEntityType;
    EntityId m_selectedEntityId = NULL_ENTITY_ID;
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

  if (m_cursorEntityId == NULL_ENTITY_ID) {
    m_core.showCursor();
  }
  else {
    m_core.hideCursor();
  }
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
  switch (m_state) {
    case State::EntitySelected: {
      if (m_selectedEntityId != NULL_ENTITY_ID) {
        setEntityColour(m_selectedEntityId, { 1.f, 1.f, 1.f, 1.f });
        m_selectedEntityId = NULL_ENTITY_ID;
      }
      if (m_cursorEntityId != NULL_ENTITY_ID) {
        m_core.engine().ecs().removeEntity(m_cursorEntityId);
        m_cursorEntityId = NULL_ENTITY_ID;
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
  m_core.hideCursor();

  m_core.loadPrefab(name);

  auto& factory = m_core.engine().entityFactory();

  auto transform = m_core.getCursorTransform();
  m_cursorEntityId = factory.constructGhostEntity(m_core.engine().worldGrid().root(), name,
   transform, GHOST_ENTITY_COLOUR);
  m_cursorEntityType = name;
}

void SceneEditModeImpl::instantiateActivePrefab()
{
  ASSERT(m_state == State::PrefabSelected, "Bad state transition");
  cancelActivePrefab();

  auto& factory = m_core.engine().entityFactory();
  auto transform = m_core.getCursorTransform();
  auto pos = getTranslation(transform);
  auto cell = cellFromPosition(pos);

  int sliceIdx = 1; // TODO
  auto& slice = m_worldState.slices[{ cell[0], cell[1], sliceIdx }];
  assert(slice.handle.id() != NULL_RESOURCE_ID);
  EntityInfo info;
  info.id = factory.constructEntity(m_core.engine().worldGrid().root(), m_cursorEntityType,
    transform);
  info.type = m_cursorEntityType;
  info.changedFromPrefab[Systems::Spatial] = true;
  slice.entities.push_back(std::move(info));
  slice.dirty = true;

  m_eventEmitter->raise(static_cast<EventId>(Event::AddOrRemoveEntity));
}

void SceneEditModeImpl::selectEntity(EntityId id)
{
  auto& factory = m_core.engine().entityFactory();

  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();

  switch (m_state) {
    case State::EntitySelected: {
      if (m_cursorEntityId != NULL_ENTITY_ID) {
        m_core.engine().ecs().removeEntity(m_cursorEntityId);
        m_cursorEntityId = NULL_ENTITY_ID;
      }
      if (m_selectedEntityId != NULL_ENTITY_ID) {
        setEntityColour(m_selectedEntityId, { 1.f, 1.f, 1.f, 1.f });
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
  m_core.hideCursor();

  auto& camera = sysRender3d.camera();
  auto& camDir = camera.getDirection();
  auto& entityTransform = m_core.engine().ecs().system<SysSpatial>().getGlobalTransform(id);
  Vec3f entityPos = getTranslation(entityTransform);

  camera.setPosition(entityPos - camDir * m_core.getCursorDistance());

  m_core.setCursorRotationScale(getRotation3x3(entityTransform));

  m_selectedEntityId = id;

  setEntityColour(id, SELECTED_ENTITY_COLOUR);

  m_cursorEntityId = factory.constructGhostEntity(m_core.engine().worldGrid().root(), id,
    GHOST_ENTITY_COLOUR);

  //m_cursorEntityType = type;

  m_eventEmitter->raise(static_cast<EventId>(Event::EntitySelect));
}

void SceneEditModeImpl::setEntityColour(EntityId id, const Vec4f& colour)
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  auto& sysRender3d = m_core.engine().ecs().system<SysRender3d>();

  auto descendents = sysSpatial.getDescendents(id);
  sysRender3d.setEntityColour(id, colour);
  for (auto descendent : descendents) {
    sysRender3d.setEntityColour(descendent, colour);
  }
}

EntityId SceneEditModeImpl::selectedEntity() const
{
  return m_cursorEntityId;
}

void SceneEditModeImpl::applyTransform()
{
  switch (m_state) {
    case State::EntitySelected: {
      applyCurrentTransformToEntity();
      break;
    }
    case State::PrefabSelected: {
      instantiateActivePrefab();
      break;
    }
    default: break;
  }
}

void SceneEditModeImpl::cancelTransform()
{
  switch (m_state) {
    case State::EntitySelected: {
      cancelCurrentEntityTransform();
      break;
    }
    case State::PrefabSelected: {
      cancelActivePrefab();
      break;
    }
    default: break;
  }
}

void SceneEditModeImpl::applyCurrentTransformToEntity()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();

  // TODO: Entity hierarchies?
  auto& transform = sysSpatial.getGlobalTransform(m_cursorEntityId);
  sysSpatial.setLocalTransform(m_selectedEntityId, transform);

  Vec3f pos = getTranslation(transform);
  auto cell = cellFromPosition(pos);

  int sliceIdx = 1; // TODO
  auto& slice = m_worldState.slices[{ cell[0], cell[1], sliceIdx }];
  slice.dirty = true;

  auto i = std::find_if(slice.entities.begin(), slice.entities.end(),
    [this](const EntityInfo& info) { return info.id == m_selectedEntityId; });
  assert(i != slice.entities.end());

  i->changedFromPrefab[Systems::Spatial] = true;
}

void SceneEditModeImpl::cancelCurrentEntityTransform()
{
  m_state = State::None;
  m_core.showCursor();

  if (m_selectedEntityId != NULL_ENTITY_ID) {
    setEntityColour(m_selectedEntityId, { 1.f, 1.f, 1.f, 1.f });
    m_selectedEntityId = NULL_ENTITY_ID;
    m_eventEmitter->raise(static_cast<EventId>(Event::EntitySelect));
  }
  if (m_cursorEntityId != NULL_ENTITY_ID) {
    m_core.engine().ecs().removeEntity(m_cursorEntityId);
    m_cursorEntityId = NULL_ENTITY_ID;
  }
}

void SceneEditModeImpl::saveChanges()
{
  std::map<std::string, EntityId> prefabs;

  for (auto& [ index, slice ] : m_worldState.slices) {
    if (slice.dirty) {
      saveChangesToSlice(index, slice, prefabs);
      slice.dirty = false;
    }
  }

  for (auto& entry : prefabs) {
    m_core.engine().eventSystem().raiseEvent(ERequestDeletion{entry.second});
  }
}

void SceneEditModeImpl::saveChangesToSlice(const fs::path& cellDirName,
  const std::string& sliceFileName, const SliceState& slice,
  std::map<std::string, EntityId>& prefabs)
{
  auto cellDir = m_core.config().paths.worldsDir->subdirectory(fs::path{"world"} / cellDirName);

  ASSERT(cellDir->fileExists(sliceFileName), "File " << sliceFileName << " doesn't exist");

  auto xmlCellSlice = createXmlNode("cell-slice");
  auto xmlEntities = createXmlNode("entities");
  for (auto& entity : slice.entities) {
    auto iPrefab = prefabs.find(entity.type);
    EntityId prefabId = NULL_ENTITY_ID;
    if (iPrefab == prefabs.end()) {
      auto& factory = m_core.engine().entityFactory();
      auto rootId = m_core.engine().worldGrid().root();
      prefabId = factory.constructEntity(rootId, entity.type, identityMatrix<4>());
      prefabs[entity.type] = prefabId;
    }
    else {
      prefabId = iPrefab->second;
    }

    auto xmlEntity = createXmlNode("entity");
    xmlEntity->setAttribute("type", entity.type);
    if (isHashedString(entity.id)) {
      xmlEntity->setAttribute("id", getHashedString(entity.id));
    }

    for (SystemId systemId = 0; systemId < m_core.engine().ecs().numSystems(); ++systemId) {
      if (entity.changedFromPrefab[systemId]) {
        auto& system = m_core.engine().ecs().getSystem(systemId);
        auto node = system.componentToXml(entity.id, prefabId);
        if (node != nullptr) {
          xmlEntity->addChild(std::move(node));
        }
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

void SceneEditModeImpl::saveChangesToSlice(const Vec3f& index, const SliceState& slice,
  std::map<std::string, EntityId>& prefabs)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << index[0]
    << std::setw(3) << std::setfill('0') << index[1];
  auto cellDirName = ss.str();
  ss.str("");
  ss << std::setw(3) << std::setfill('0') << index[2] << ".xml";
  auto sliceFileName = ss.str();

  saveChangesToSlice(cellDirName, sliceFileName, slice, prefabs);
}

void SceneEditModeImpl::cancelActivePrefab()
{
  ASSERT(m_state == State::PrefabSelected, "Bad state transition");
  m_state = State::None;
  m_core.showCursor();

  if (m_cursorEntityId != NULL_ENTITY_ID) {
    m_core.engine().ecs().removeEntity(m_cursorEntityId);
  }
  m_cursorEntityId = NULL_ENTITY_ID;
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

  if (m_cursorEntityId != NULL_ENTITY_ID) {
    sysSpatial.setLocalTransform(m_cursorEntityId, m_core.getCursorTransform());
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
