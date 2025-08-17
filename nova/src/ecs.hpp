#include <vector>
#include <map>
#include <cstring>
#include <cassert>
#include <stdexcept>

using EntityId = size_t;
using ComponentType = size_t;

struct ComponentArray
{
  size_t componentSize;
  std::vector<char> data;
};

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
      static_assert(std::is_trivially_copyable_v<T>);
      static_assert(T::TypeId && (T::TypeId & (T::TypeId - 1)) == 0, "TypeId must be power of 2");

      auto componentSize = sizeof(T);

      auto i = m_componentData.find(T::TypeId);
      if (i == m_componentData.end()) {
        i = m_componentData.insert({T::TypeId, ComponentArray{
          .componentSize = componentSize,
          .data{}
        }}).first;
      }

      auto& array = i->second;
      array.data.resize(array.data.size() + componentSize);

      assert(m_entityIds.size() == m_indices.size());
    }

    template<typename T>
    T* getComponents()
    {
      auto i = m_componentData.find(T::TypeId);
      if (i == m_componentData.end()) {
        throw std::runtime_error("Entity does not have component of given type");
      }

      auto& array = i->second;
      return reinterpret_cast<T*>(array.data.data());
    }

    template<typename T>
    T& getComponent(EntityId entityId)
    {
      return getComponents<T>()[entityPosition(entityId)];
    }

    void remove(EntityId entityId)
    {
      auto i = m_indices.find(entityId);
      if (i == m_indices.end()) {
        return;
      }

      assert(m_entityIds.size() == m_indices.size());

      size_t entityIndex = i->second;
      size_t lastIndex = m_indices[m_indices.size() - 1];

      for (auto entry : m_componentData) {
        auto& components = entry.second;
        size_t size = components.componentSize;

        if (entityIndex != lastIndex) {
          size_t entityStart = entityIndex * size;
          size_t lastStart = components.data.size() - size;

          assert(lastStart == lastIndex * size);

          memcpy(components.data.data() + entityStart, components.data.data() + lastStart, size);
        }

        components.data.resize(components.data.size() - size);
      }

      size_t lastEntityId = m_entityIds.at(lastIndex);

      m_indices.erase(entityId);
      if (entityIndex != lastIndex) {
        m_indices[lastEntityId] = entityIndex;
        std::swap(m_entityIds[entityIndex], m_entityIds[lastIndex]);
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
    std::map<ComponentType, ComponentArray> m_componentData;
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
    std::vector<ComponentArrayGroup*> getComponents()
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
    T& getComponent(EntityId entityId)
    {
      auto i = m_archetypes.find(entityId);
      if (i == m_archetypes.end()) {
        throw std::runtime_error("No such entity");
      }
      return m_groups.at(i->second).getComponent<T>(entityId);
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
