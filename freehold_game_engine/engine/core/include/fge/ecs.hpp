#pragma once

#include "strings.hpp"
#include "units.hpp"
#include "component_store.hpp"
#include <set>
#include <memory>

namespace fge
{

using SystemId = uint32_t;

using EntityIdSet = std::set<EntityId>;

class Event;
struct InputState;

class System
{
  public:
    virtual void removeEntity(EntityId entityId) = 0;
    virtual bool hasEntity(EntityId entityId) const = 0;
    virtual void update(Tick tick, const InputState& inputState) = 0;
    virtual void processEvent(const Event& event) = 0;

    virtual ~System() = default;
};

using SystemPtr = std::unique_ptr<System>;

class Ecs
{
  public:
    virtual void addSystem(SystemId id, SystemPtr system) = 0;
    virtual System& getSystem(SystemId id) = 0;
    virtual const System& getSystem(SystemId id) const = 0;
    virtual void update(Tick tick, const InputState& inputState) = 0;
    virtual void processEvent(const Event& event) = 0;
    // Warning: This will immediately delete the entity. Consider raising ERequestDeletion instead.
    virtual void removeEntity(EntityId entityId) = 0;
    virtual ComponentStore& componentStore() = 0;
    virtual const ComponentStore& componentStore() const = 0;

    template<std::derived_from<System> T>
    T& system()
    {
      return dynamic_cast<T&>(getSystem(T::id));
    }

    virtual ~Ecs() = default;
};

using EcsPtr = std::unique_ptr<Ecs>;

template<typename T>
inline void assertHasComponent(const ComponentStore& store, EntityId id)
{
  ASSERT(store.hasComponentForEntity<T>(id),
    typeid(T).name() << " component missing for entity " << id);
}

class Logger;

EcsPtr createEcs(Logger& logger);

} // namespace fge
