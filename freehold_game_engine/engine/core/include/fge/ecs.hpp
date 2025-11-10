#pragma once

#include "strings.hpp"
#include "units.hpp"
#include "component_store.hpp"
#include <set>
#include <memory>

// Naming conventions
//
// Systems inherit System and are prefixed with Sys, e.g. SysRender. They should define a static
// data member of type SystemId that is unique to the class.
// Components stored in the component store are prefixed with C and should be plain-old-data.
// Components should define a static TypeId data member of type ComponentType unique to the class.
// The component store is used to facilitate fast iteration over components within a system's
// update() method. Systems don't have to use the component store at all.
// Entity data passed into the system (and possibly used to populate component store components)
// are prefixed with D, e.g. DSpatial.

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

    template<std::derived_from<System> T>
    const T& system() const
    {
      return dynamic_cast<const T&>(getSystem(T::id));
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
