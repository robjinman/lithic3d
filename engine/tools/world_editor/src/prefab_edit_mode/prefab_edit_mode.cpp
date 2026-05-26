#include "prefab_edit_mode/prefab_edit_mode.hpp"
#include "editor_core.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;

namespace
{

struct SuspendResumeState
{
  Vec3f cameraPosition = metresToWorldUnits(Vec3f{ 0.f, 0.f, 5.f });
  Vec3f cameraDirection = { 0.f, 0.f, -1.f };
  Mat3x3f cursorRotationScale = scaleMatrix<3>(WORLD_UNITS_PER_METRE, false);
  float cursorDistance = metresToWorldUnits(10.f);
};

class PrefabEditModeImpl : public PrefabEditMode
{
  public:
    PrefabEditModeImpl(EditorCore& core);

    void activate() override;
    void deactivate() override;
    void saveChanges() override;
    void update() override;
    void setActivePrefab(const std::string& prefab) override;

    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;

  private:
    EditorCore& m_core;
    EntityId m_rootId = NULL_ENTITY_ID;
    EntityId m_entityId = NULL_ENTITY_ID;
    SuspendResumeState m_suspendResumeState;
    Vec2f m_prevMousePos;

    void constructRoot();
    void processKeyboardInput();
};

PrefabEditModeImpl::PrefabEditModeImpl(EditorCore& core)
  : m_core(core)
{
  constructRoot();
}

void PrefabEditModeImpl::constructRoot()
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

void PrefabEditModeImpl::activate()
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  sysSpatial.setEnabled(m_rootId, true);

  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();

  camera.setPosition(m_suspendResumeState.cameraPosition);
  camera.setDirection(m_suspendResumeState.cameraDirection);

  m_core.setCursorRotationScale(m_suspendResumeState.cursorRotationScale);
  m_core.setCursorDistance(m_suspendResumeState.cursorDistance);
}

void PrefabEditModeImpl::deactivate()
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

void PrefabEditModeImpl::setActivePrefab(const std::string& prefab)
{
  auto& engine = m_core.engine();

  if (m_entityId != NULL_ENTITY_ID) {
    engine.eventSystem().raiseEvent(ERequestDeletion{m_entityId});
  }

  auto transform = scaleMatrix<4>(WORLD_UNITS_PER_METRE, true);
  m_entityId = engine.entityFactory().constructEntity(m_rootId, prefab, transform);
}

void PrefabEditModeImpl::onKeyDown(KeyboardKey key)
{

}

void PrefabEditModeImpl::onKeyUp(KeyboardKey key)
{
  
}

void PrefabEditModeImpl::onMouseLeftBtnDown()
{

}

void PrefabEditModeImpl::onMouseLeftBtnUp()
{

}

void PrefabEditModeImpl::onMouseMove(float x, float y)
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

void PrefabEditModeImpl::processKeyboardInput()
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

void PrefabEditModeImpl::saveChanges()
{
  // TODO
}

void PrefabEditModeImpl::update()
{
  processKeyboardInput();
}

} // namespace

PrefabEditModePtr createPrefabEditMode(EditorCore& core)
{
  return std::make_unique<PrefabEditModeImpl>(core);
}
