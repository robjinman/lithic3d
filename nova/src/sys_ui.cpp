#include "sys_ui.hpp"
#include "input_state.hpp"
#include "logger.hpp"
#include "sys_spatial.hpp"
#include <map>

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
  UiItemCallback onGainFocus = []() {};
  UiItemCallback onLoseFocus = []() {};
  UiItemCallback onPrime = []() {};
  UiItemCallback onActivate = []() {};
  UiItemCallback onCancel = []() {};
  std::array<EntityId, 4> slots;
  bool primed = false;
};

enum class KeyInput
{
  // Indices into ItemData::slots array
  Left = 0,
  Right,
  Up,
  Down,

  Enter
};

class SysUiImpl : public SysUi
{
  public:
    SysUiImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event&) override {}

    void addEntity(EntityId id, const UiData& data) override;
    void setActiveGroup(GroupId id) override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    std::map<EntityId, ItemData> m_componentData;
    std::unordered_map<GroupId, ItemGroup> m_groups;
    GroupId m_activeGroup = 0;
    InputState m_prevInputState;

    void processMouseInput(const Vec2f& mousePos, bool mousePressed, bool mouseReleased);
    void processKeyPress(KeyInput key);

    // State transitions
    void activateItem(EntityId id, ItemData& compData);
    void cancelItem(EntityId id, ItemData& compData);
    void primeItem(EntityId id, ItemData& compData);
    void unfocusItem(EntityId id, ItemData& compData);
    void focusItem(EntityId id, ItemData& compData);
};

SysUiImpl::SysUiImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_eventSystem(eventSystem)
{}

void SysUiImpl::addEntity(EntityId id, const UiData& data)
{
  auto& component = m_ecs.componentStore().component<CUi>(id);
  component = CUi{};

  m_componentData.insert({ id, ItemData{
    .group = data.group,
    .onGainFocus = data.onGainFocus,
    .onLoseFocus = data.onLoseFocus,
    .onPrime = data.onPrime,
    .onActivate = data.onActivate,
    .onCancel = data.onCancel,
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

void SysUiImpl::setActiveGroup(GroupId id)
{
  if (m_activeGroup != 0) {
    auto& group = m_groups.at(m_activeGroup);
    if (group.focusedItem != NULL_ENTITY) {
      unfocusItem(group.focusedItem, m_componentData.at(group.focusedItem));
    }
  }

  m_activeGroup = id;

  auto& group = m_groups.at(m_activeGroup);
  assert(!group.items.empty());

  auto firstItem = *group.items.begin();
  focusItem(firstItem, m_componentData.at(firstItem));
}

void SysUiImpl::activateItem(EntityId id, ItemData& compData)
{
  compData.primed = false;
  m_eventSystem.queueEvent(std::make_unique<EUiItemStateChange>(g_strUiItemActivate, id));
  compData.onActivate();
}

void SysUiImpl::cancelItem(EntityId id, ItemData& compData)
{
  compData.primed = false;
  m_eventSystem.queueEvent(std::make_unique<EUiItemStateChange>(g_strUiItemCancel, id));
  compData.onCancel();
}

void SysUiImpl::primeItem(EntityId id, ItemData& compData)
{
  compData.primed = true;
  m_eventSystem.queueEvent(std::make_unique<EUiItemStateChange>(g_strUiItemPrime, id));
  compData.onPrime();
}

void SysUiImpl::unfocusItem(EntityId id, ItemData& compData)
{
  if (compData.group != 0) {
    auto& group = m_groups.at(compData.group);
    if (group.focusedItem == id) {
      group.focusedItem = NULL_ENTITY;
    }
  }

  m_eventSystem.queueEvent(std::make_unique<EUiItemStateChange>(g_strUiItemLoseFocus, id));
  compData.onLoseFocus();
}

void SysUiImpl::focusItem(EntityId id, ItemData& compData)
{
  if (compData.group != m_activeGroup) {
    setActiveGroup(compData.group); // Null group ID is OK
  }

  if (compData.group != 0) {
    auto& group = m_groups.at(compData.group);
    if (group.focusedItem != 0) {
      unfocusItem(group.focusedItem, m_componentData.at(group.focusedItem));
    }
    group.focusedItem = id;
  }

  m_eventSystem.queueEvent(std::make_unique<EUiItemStateChange>(g_strUiItemGainFocus, id));
  compData.onGainFocus();
}

void SysUiImpl::processMouseInput(const Vec2f& mousePos, bool mousePressed, bool mouseReleased)
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

        if (componentData.primed && mouseReleased) {
          activateItem(id, componentData);
        }
        else if (!componentData.primed && mousePressed) {
          primeItem(id, componentData);
        }
        else if (!uiComp.mouseOver) {
          uiComp.mouseOver = true;
          focusItem(id, componentData);
        }
      }
      else {
        if (uiComp.mouseOver) {
          auto& componentData = m_componentData.at(id);
          uiComp.mouseOver = false;

          if (componentData.primed) {
            cancelItem(id, componentData);
          }
        }
      }
    }
  }
}

void SysUiImpl::processKeyPress(KeyInput key)
{
  if (m_activeGroup == 0) {
    return;
  }

  auto& group = m_groups.at(m_activeGroup);

  switch (key) {
    case KeyInput::Left:
    case KeyInput::Up:
    case KeyInput::Right:
    case KeyInput::Down: {
      auto& currentItem = m_componentData.at(group.focusedItem);
      auto nextItemId = currentItem.slots[static_cast<size_t>(key)];

      if (nextItemId != NULL_ENTITY) {
        unfocusItem(group.focusedItem, currentItem);
        focusItem(nextItemId, m_componentData.at(nextItemId));
      }

      break;
    }
    case KeyInput::Enter: {
      if (group.focusedItem != NULL_ENTITY) {
        activateItem(group.focusedItem, m_componentData.at(group.focusedItem));
      }

      break;
    }
  }
}

std::optional<KeyInput> getKeyPress(const InputState& prevState, const InputState& state)
{
  if (!prevState.left && state.left) {
    return KeyInput::Left;
  }
  else if (!prevState.right && state.right) {
    return KeyInput::Right;
  }
  else if (!prevState.up && state.up) {
    return KeyInput::Up;
  }
  else if (!prevState.down && state.down) {
    return KeyInput::Down;
  }
  else if (!prevState.enter && state.enter) {
    return KeyInput::Enter;
  }
  return std::nullopt;
}

void SysUiImpl::update(Tick tick, const InputState& inputState)
{
  bool mousePressed = !m_prevInputState.mouseBtn && inputState.mouseBtn;
  bool mouseReleased = m_prevInputState.mouseBtn && !inputState.mouseBtn;
  bool mouseMoved = m_prevInputState.mousePos != inputState.mousePos;
  bool mouseStateChanged = mousePressed || mouseReleased || mouseMoved;

  if (mouseStateChanged) {
    processMouseInput(inputState.mousePos, mousePressed, mouseReleased);
  }

  auto key = getKeyPress(m_prevInputState, inputState);

  if (key.has_value()) {
    processKeyPress(key.value());
  }

  m_prevInputState = inputState;
}

} // namespace

SysUiPtr createSysUi(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysUiImpl>(ecs, eventSystem, logger);
}
