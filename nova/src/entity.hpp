#pragma once

#include <memory>
#include <functional>
#include <map>
#include <set>

using HashedString = uint32_t;

constexpr HashedString hashStringN(const char* str, std::size_t length)
{
  uint32_t hash = 0x811c9dc5u;
  for (std::size_t i = 0; i < length; ++i) {
    hash ^= static_cast<HashedString>(str[i]);
    hash *= 0x01000193u;
  }
  return hash;
}

template<std::size_t N>
constexpr HashedString hashString(const char (&str)[N])
{
  return hashStringN(str, N - 1);
}

class Component
{
  public:
    virtual const std::set<HashedString>& subscriptions() const = 0;
    virtual void update() = 0;
    virtual void handleEvent(const GameEvent& event) = 0;

    virtual ~Component() {}
};

using ComponentPtr = std::unique_ptr<Component>;

struct Event
{
  public:
    Event(HashedString type)
      : m_type(type) {}

    virtual HashedString type() const;

  private:
    HashedString m_type;
};

struct EPlayerMoved : public Event
{
  Vec2i fromPos;
  Vec2i toPos;
};

struct EItemCollected : public Event
{
  EItemCollected(EntityId entityId, int value);
};

class ERequestDeletion : public Event
{
  public:
    ERequestDeletion(EntityId entityId);
};

constexpr HashedString strGame = hashString("game");

class GameEvent : public Event
{
  public:
    GameEvent(HashedString name)
      : Event(strGame)
      , name(name) {}

    GameEvent(HashedString name, const std::set<EntityId>& targets)
      : Event(strGame)
      , name(name)
      , targets(targets) {}

    HashedString name;
    std::set<EntityId> targets;
};

using EventHandlerFn = std::function<void(const Event&)>;

class EventSystem
{
  public:
    class Handle
    {

    };

    Handle listen(HashedString category, EventHandlerFn handler);
    Handle listen(HashedString category, HashedString type, EventHandlerFn handler);
    void fireEvent(const Event& event);
    void forget(const Handle& handle);
};

using EventSystemPtr = std::unique_ptr<EventSystem>;

EventSystemPtr createEventSystem();

using EntityId = size_t;

class Entity
{
  public:
    Entity()
      : m_id(m_nextId++) {}

    EntityId id() const { return m_id; }

    virtual const std::set<HashedString>& subscriptions() const;
    virtual void update();
    virtual void draw() const;
    virtual void handleEvent(const Event& event);

    virtual ~Entity() = 0;

  private:
    static EntityId m_nextId;

    EntityId m_id;
};

using EntityPtr = std::unique_ptr<Entity>;

class Wanderer : public Entity
{
  public:
    const std::set<HashedString>& subscriptions() const override
    {
      static std::set<HashedString> subs{
        hashString("explode")
      };
    }

    void update() override
    {

    }

    void draw() const override
    {

    }

    void handleEvent(const Event& event) override
    {

    }

  private:
    CRenderPtr m_renderComp;
};

void foo()
{
  constexpr HashedString strGame = hashString("game");

  auto eventSystem = createEventSystem();

  std::map<EntityId, EntityPtr> entities;
  std::map<HashedString, std::set<EntityId>> subscriptions;

  EntityPtr entity = std::make_unique<Wanderer>();
  auto& subs = entity->subscriptions();
  for (auto type : subs) {
    subscriptions[type].insert(entity->id());
  }

  eventSystem->listen(strGame, [&](const Event& e) {
    auto& gameEvent = dynamic_cast<const GameEvent&>(e);

    if (gameEvent.targets.empty()) {
      for (auto id : subscriptions[gameEvent.name]) {
        entities[id]->handleEvent(gameEvent);
      }
    }
    else {
      for (auto id : gameEvent.targets) {
        //auto& subs = entities[id]->subscriptions();
        //if (subs.contains(gameEvent.name)) {
          entities[id]->handleEvent(gameEvent);
        //}
      }
    }
  });
}
