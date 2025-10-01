#include "sys_ui.hpp"
#include "input.hpp"
#include "logger.hpp"
#include "sys_spatial.hpp"
#include <map>
#include <algorithm>

namespace
{

struct ItemGroup
{
  EntityId focusedItem = NULL_ENTITY;
  std::set<EntityId> items;
};

struct ItemData
{
  SysUi::GroupId group = 0;
  std::set<UserInput> inputFilter{};
  std::function<void()> onGainFocus = []() {};
  std::function<void()> onLoseFocus = []() {};
  std::function<void(const UserInput& input)> onInputBegin = [](const UserInput&) {};
  std::function<void(const UserInput& input)> onInputEnd = [](const UserInput&) {};
  std::function<void()> onInputCancel = []() {};
  std::array<EntityId, 4> slots{};
  bool primed = false;
};

size_t keyToSlotIndex(KeyboardKey key)
{
  switch (key) {
    case KeyboardKey::Left: return 0;
    case KeyboardKey::Right: return 1;
    case KeyboardKey::Up: return 2;
    case KeyboardKey::Down: return 3;
    default:
      EXCEPTION("Expected direction key");
      break;
  }
}

class SysUiImpl : public SysUi
{
  public:
    SysUiImpl(Ecs& ecs, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event&) override {}

    void addEntity(EntityId id, const UiData& data) override;
    void setActiveGroup(GroupId id, EntityId focusedItem) override;

    void sendInputEnd(EntityId id, UserInput input) override;
    void sendInputCancel(EntityId id) override;
    void sendInputBegin(EntityId id, UserInput input) override;
    void sendUnfocus(EntityId id) override;
    void sendFocus(EntityId id) override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    std::map<EntityId, ItemData> m_componentData;
    std::unordered_map<GroupId, ItemGroup> m_groups;
    GroupId m_activeGroup = 0;
    InputState m_prevInputState;

    void processMouseInput(const Vec2f& mousePos, std::optional<MouseButton> mousePressed,
      std::optional<MouseButton> mouseReleased);
    void processKeyPress(KeyboardKey key);
    void processKeyRelease(KeyboardKey key);
};

SysUiImpl::SysUiImpl(Ecs& ecs, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
{}

void SysUiImpl::addEntity(EntityId id, const UiData& data)
{
  auto& component = m_ecs.componentStore().component<CUi>(id);
  component = CUi{};

  m_componentData.insert({ id, ItemData{
    .group = data.group,
    .inputFilter = data.inputFilter,
    .onGainFocus = data.onGainFocus,
    .onLoseFocus = data.onLoseFocus,
    .onInputBegin = data.onInputBegin,
    .onInputEnd = data.onInputEnd,
    .onInputCancel = data.onInputCancel,
    .slots{
      data.leftSlot,
      data.rightSlot,
      data.topSlot,
      data.bottomSlot
    },
    .primed = false
  }});

  if (data.group != 0) {
    auto i = m_groups.find(data.group);
    if (i == m_groups.end()) {
      m_groups.insert({ data.group, ItemGroup{} });
      i = m_groups.find(data.group);
    }
    i->second.items.insert(id);

    if (m_activeGroup == 0) {
      m_activeGroup = data.group;
    }
  }
}

void SysUiImpl::removeEntity(EntityId id)
{
  auto i = m_componentData.find(id);

  if (i != m_componentData.end()) {
    auto& component = i->second;

    if (component.group != 0) {
      auto& group = m_groups.at(component.group);
      group.items.erase(id);
      if (group.items.empty()) {
        m_groups.erase(component.group);
      }

      if (m_activeGroup == component.group) {
        m_activeGroup = 0;
      }
    }

    m_componentData.erase(i);
  }
}

bool SysUiImpl::hasEntity(EntityId entityId) const
{
  return m_ecs.componentStore().hasComponentForEntity<CUi>(entityId);
}

void SysUiImpl::setActiveGroup(GroupId id, EntityId focusedItem)
{
  if (m_activeGroup != 0) {
    auto& group = m_groups.at(m_activeGroup);
    if (group.focusedItem != NULL_ENTITY) {
      auto focused = group.focusedItem;
      sendUnfocus(group.focusedItem);
      group.focusedItem = focused;
    }
  }

  m_activeGroup = id;

  auto& group = m_groups.at(m_activeGroup);
  assert(!group.items.empty());

  if (focusedItem != NULL_ENTITY) {
    sendFocus(focusedItem);
  }
  else if (group.focusedItem != NULL_ENTITY) {
    sendFocus(group.focusedItem);
  }
  else {
    sendFocus(*group.items.begin());
  }
}

void SysUiImpl::sendInputEnd(EntityId id, UserInput input)
{
  auto& compData = m_componentData.at(id);

  if (compData.inputFilter.contains(input)) {
    compData.primed = false;
    compData.onInputEnd(input);
  }
}

void SysUiImpl::sendInputCancel(EntityId id)
{
  auto& compData = m_componentData.at(id);

  compData.primed = false;
  compData.onInputCancel();
}

void SysUiImpl::sendInputBegin(EntityId id, UserInput input)
{
  auto& compData = m_componentData.at(id);

  if (compData.inputFilter.contains(input)) {
    compData.primed = true;
    compData.onInputBegin(input);
  }
}

void SysUiImpl::sendUnfocus(EntityId id)
{
  auto& compData = m_componentData.at(id);

  if (compData.group != 0) {
    auto& group = m_groups.at(compData.group);
    if (group.focusedItem == id) {
      group.focusedItem = NULL_ENTITY;
    }
  }

  compData.onLoseFocus();
}

void SysUiImpl::sendFocus(EntityId id)
{
  auto& compData = m_componentData.at(id);

  if (compData.group == 0) {
    EXCEPTION("Entity must be part of a group to receive focus");
  }

  if (compData.group != m_activeGroup) {
    setActiveGroup(compData.group, id);
  }

  if (compData.group != 0) {
    auto& group = m_groups.at(compData.group);
    if (group.focusedItem != 0) {
      sendUnfocus(group.focusedItem);
    }
    group.focusedItem = id;
  }

  compData.onGainFocus();
}

void SysUiImpl::processMouseInput(const Vec2f& mousePos, std::optional<MouseButton> mousePressed,
  std::optional<MouseButton> mouseReleased)
{
  auto groups = m_ecs.componentStore().components<CSpatialFlags, CGlobalTransform, CUi>();

  for (auto& group : groups) {
    auto flags = group.components<CSpatialFlags>();
    auto transforms = group.components<CGlobalTransform>();
    auto uiComps = group.components<CUi>();
    auto n = group.numEntities();

    // Fast loop. Don't access m_componentData on every iteration.
    for (size_t i = 0; i < n; ++i) {
      if (!(flags[i].enabled && flags[i].parentEnabled)) {
        continue;
      }

      auto& t = transforms[i];
      auto& uiComp = uiComps[i];
      EntityId id = group.entityIds()[i];

      Vec2f pos{ t.transform.at(0, 3), t.transform.at(1, 3) };
      Vec2f size{ t.transform.at(0, 0), t.transform.at(1, 1) };

      if (inRange(mousePos[0], pos[0], pos[0] + size[0]) &&
        inRange(mousePos[1], pos[1], pos[1] + size[1])) {

        auto& componentData = m_componentData.at(id);

        if (componentData.primed && mouseReleased.has_value()) {
          sendInputEnd(id, mouseReleased.value());
        }
        else if (!componentData.primed && mousePressed.has_value()) {
          sendInputBegin(id, mousePressed.value());
        }
        else if (!uiComp.mouseOver) {
          uiComp.mouseOver = true;
          sendFocus(id);
        }
      }
      else {
        if (uiComp.mouseOver) {
          auto& componentData = m_componentData.at(id);
          uiComp.mouseOver = false;

          if (componentData.primed) {
            sendInputCancel(id);
          }
        }
      }
    }
  }
}

void SysUiImpl::processKeyPress(KeyboardKey key)
{
  if (m_activeGroup == 0) {
    return;
  }

  auto& group = m_groups.at(m_activeGroup);

  switch (key) {
    case KeyboardKey::Left:
    case KeyboardKey::Up:
    case KeyboardKey::Right:
    case KeyboardKey::Down: {
      auto& currentItem = m_componentData.at(group.focusedItem);
      auto nextItemId = currentItem.slots[keyToSlotIndex(key)];

      if (nextItemId != NULL_ENTITY) {
        sendUnfocus(group.focusedItem);
        sendFocus(nextItemId);
        break;
      }

      [[fallthrough]];
    }
    default: {
      if (group.focusedItem != NULL_ENTITY) {
        sendInputBegin(group.focusedItem, key);
      }

      break;
    }
  }
}

void SysUiImpl::processKeyRelease(KeyboardKey key)
{
  if (m_activeGroup == 0) {
    return;
  }

  auto& group = m_groups.at(m_activeGroup);

  if (group.focusedItem != NULL_ENTITY) {
    sendInputEnd(group.focusedItem, key);
  }
}

template<typename T>
std::optional<T> firstDifference(const std::set<T>& A, const std::set<T>& B)
{
  std::set<T> diffs;
  std::set_difference(A.begin(), A.end(), B.begin(), B.end(), std::inserter(diffs, diffs.begin()));
  if (!diffs.empty()) {
    return *diffs.begin();
  }
  return std::nullopt;
}

std::optional<UserInput> getInput(const InputState& prevState, const InputState& state)
{
  auto mouseInput = firstDifference(state.mouseButtonsPressed, prevState.mouseButtonsPressed);
  if (mouseInput.has_value()) {
    return mouseInput.value();
  }

  auto key = firstDifference(state.keysPressed, prevState.keysPressed);
  if (key.has_value()) {
    return key.value();
  }

  auto btn = firstDifference(state.gamepadButtonsPressed, prevState.gamepadButtonsPressed);
  if (btn.has_value()) {
    return btn.value();
  }

  return std::nullopt;
}

void SysUiImpl::update(Tick, const InputState& inputState)
{
  auto inputBegin = getInput(m_prevInputState, inputState);
  auto inputEnd = getInput(inputState, m_prevInputState);

  // TODO: Gamepad input

  std::optional<MouseButton> mouseButtonPressed = std::nullopt;
  std::optional<MouseButton> mouseButtonReleased = std::nullopt;

  if (inputBegin.has_value() && std::holds_alternative<MouseButton>(inputBegin.value())) {
    mouseButtonPressed = std::get<MouseButton>(inputBegin.value());
  }

  if (inputEnd.has_value() && std::holds_alternative<MouseButton>(inputEnd.value())) {
    mouseButtonReleased = std::get<MouseButton>(inputEnd.value());
  }

  bool mouseMoved = m_prevInputState.mousePos != inputState.mousePos;
  bool mouseStateChanged =
    mouseButtonPressed.has_value() || mouseButtonReleased.has_value() || mouseMoved;

  if (mouseStateChanged) {
    processMouseInput(inputState.mousePos, mouseButtonPressed, mouseButtonReleased);
  }

  std::optional<KeyboardKey> keyPress = std::nullopt;
  std::optional<KeyboardKey> keyRelease = std::nullopt;

  if (inputBegin.has_value() && std::holds_alternative<KeyboardKey>(inputBegin.value())) {
    keyPress = std::get<KeyboardKey>(inputBegin.value());
  }

  if (inputEnd.has_value() && std::holds_alternative<KeyboardKey>(inputEnd.value())) {
    keyRelease = std::get<KeyboardKey>(inputEnd.value());
  }

  if (keyRelease.has_value()) {
    processKeyRelease(keyRelease.value());
  }
  else if (keyPress.has_value()) {
    processKeyPress(keyPress.value());
  }

  m_prevInputState = inputState;
}

} // namespace

SysUiPtr createSysUi(Ecs& ecs, Logger& logger)
{
  return std::make_unique<SysUiImpl>(ecs, logger);
}
