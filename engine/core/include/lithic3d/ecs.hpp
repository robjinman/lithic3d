#pragma once

#include "strings.hpp"
#include "units.hpp"
#include "component_store.hpp"
#include <set>
#include <memory>

namespace lithic3d
{

using SystemId = uint32_t;

class Event;
class XmlNode;
struct InputState;

class ComponentData
{
  public:
    virtual size_t typeId() const = 0;
    virtual ~ComponentData() = default;
};

using ComponentDataPtr = std::unique_ptr<ComponentData>;

template<typename T>
class ComponentDataWrapper : public ComponentData
{
  public:
    ComponentDataWrapper(const T& data)
      : m_data(data)
    {}

    size_t typeId() const override
    {
      return typeid(T).hash_code();
    }

    T& data()
    {
      return m_data;
    }

    const T& data() const
    {
      return m_data;
    }

  private:
    T m_data;
};

class System
{
  public:
    virtual const std::string& name() const = 0;
    virtual void extractComponentSpecs(const ComponentData& data,
      std::vector<ComponentSpec>& specs) const = 0;
    // TODO: Rename async?
    virtual ComponentDataPtr constructComponentData(const XmlNode& data) const = 0;
    virtual ComponentDataPtr constructComponentDataWithModifications(const ComponentData& base,
      const XmlNode& changes) const = 0;
    virtual void addEntity(EntityId id, const ComponentData& data) = 0;
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
    virtual uint32_t numSystems() const = 0;
    virtual System& getSystem(SystemId id) = 0;
    virtual const System& getSystem(SystemId id) const = 0;
    virtual void update(Tick tick, const InputState& inputState) = 0;
    virtual void processEvent(const Event& event) = 0;
    // Warning: This will immediately delete the entity. Consider raising ERequestDeletion instead.
    // TODO: Delete?
    virtual void removeEntity(EntityId entityId) = 0;
    virtual ComponentStore& componentStore() = 0;
    virtual const ComponentStore& componentStore() const = 0;
    virtual EntityIdAllocator& idGen() = 0;

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

} // namespace lithic3d
