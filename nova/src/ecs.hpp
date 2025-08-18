#include <vector>
#include <map>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <memory>

using EntityId = size_t;
using ComponentType = size_t;

struct IComponentArray
{
  virtual size_t componentSize() const = 0;
  virtual void remove(size_t index) = 0;
  virtual char* data() = 0;
  virtual void allocate() = 0;

  virtual ~IComponentArray() = default;
};

template<typename T>
class ComponentArray : public IComponentArray
{
  static_assert(std::is_default_constructible_v<T>);
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(T::TypeId && (T::TypeId & (T::TypeId - 1)) == 0, "TypeId must be power of 2");

  public:
    size_t componentSize() const override
    {
      return sizeof(T);
    }

    void remove(size_t index) override
    {
      std::swap(m_data[index], m_data[m_data.size() - 1]);
      m_data.pop_back();
    }

    char* data() override
    {
      return reinterpret_cast<char*>(m_data.data());
    }

    void allocate() override
    {
      m_data.resize(m_data.size() + 1);
    }

  private:
    std::vector<T> m_data;
};

using ComponentArrayPtr = std::unique_ptr<IComponentArray>;

class ComponentArrayGroup
{
  public:
    template<typename... Ts>
    void allocate(EntityId entityId)
    {
      (allocate<Ts>(), ...);
      m_entityIds.push_back(entityId);
      m_indices.insert({ entityId, m_entityIds.size() - 1 });
    }

    template<typename T>
    void allocate()
    {
      auto i = m_componentData.find(T::TypeId);
      if (i == m_componentData.end()) {
        i = m_componentData.insert({ T::TypeId, std::make_unique<ComponentArray<T>>() }).first;
      }

      i->second->allocate();

      assert(m_entityIds.size() == m_indices.size());
    }

    template<typename T>
    T* components()
    {
      auto i = m_componentData.find(T::TypeId);
      if (i == m_componentData.end()) {
        throw std::runtime_error("Entity does not have component of given type");
      }

      auto& array = i->second;
      return reinterpret_cast<T*>(array->data());
    }

    template<typename T>
    T& component(EntityId entityId)
    {
      return components<T>()[entityPosition(entityId)];
    }

    void remove(EntityId entityId)
    {
      auto i = m_indices.find(entityId);
      if (i == m_indices.end()) {
        return;
      }

      assert(m_entityIds.size() == m_indices.size());

      size_t entityIndex = i->second;
      size_t lastEntityIndex = m_entityIds.size() - 1;
      EntityId lastEntityId = m_entityIds[lastEntityIndex];

      for (auto& [type, components] : m_componentData) {
        components->remove(entityIndex);
      }

      m_indices.erase(entityId);
      if (entityId != lastEntityId) {
        m_indices[lastEntityId] = entityIndex;
        m_entityIds[entityIndex] = lastEntityId;
      }
      m_entityIds.pop_back();

      assert(m_entityIds.size() == m_indices.size());
    }

    const std::vector<EntityId>& entityIds() const
    {
      return m_entityIds;
    }

    size_t numEntities() const
    {
      assert(m_entityIds.size() == m_indices.size());
      return m_entityIds.size();
    }

    bool hasEntity(EntityId entityId) const
    {
      return m_indices.contains(entityId);
    }

    size_t entityPosition(EntityId entityId) const
    {
      auto i = m_indices.find(entityId);
      if (i == m_indices.end()) {
        throw std::runtime_error("No such entity");
      }
      return i->second;
    }

  private:
    std::map<ComponentType, ComponentArrayPtr> m_componentData;
    std::vector<EntityId> m_entityIds;
    std::map<EntityId, size_t> m_indices;
};

using Archetype = size_t;

class World
{
  public:
    template<typename... Ts>
    EntityId allocate()
    {
      EntityId entityId = m_nextId++;

      Archetype archetype = (Ts::TypeId | ...);

      m_groups[archetype].allocate<Ts...>(entityId);
      m_archetypes[entityId] = archetype;

      return entityId;
    }

    template<typename... Ts>
    std::vector<ComponentArrayGroup*> components()
    {
      std::vector<ComponentArrayGroup*> groups;

      Archetype mask = (Ts::TypeId | ...);

      for (auto& group : m_groups) {
        if ((group.first & mask) == mask) {
          groups.push_back(&group.second);
        }
      }

      return groups;
    }

    template<typename T>
    T& component(EntityId entityId)
    {
      auto i = m_archetypes.find(entityId);
      if (i == m_archetypes.end()) {
        throw std::runtime_error("No such entity");
      }
      return m_groups.at(i->second).component<T>(entityId);
    }

    bool hasEntity(EntityId entityId) const
    {
      return m_archetypes.contains(entityId);
    }

    void remove(EntityId entityId)
    {
      auto i = m_archetypes.find(entityId);
      if (i != m_archetypes.end()) {
        m_groups.at(i->second).remove(entityId);
        m_archetypes.erase(i);
      }
    }

  private:
    EntityId m_nextId = 0;
    std::map<Archetype, ComponentArrayGroup> m_groups;
    std::map<EntityId, Archetype> m_archetypes;
};
