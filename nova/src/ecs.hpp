#pragma once

#include <vector>
#include <map>
#include <cassert>
#include <stdexcept>
#include <memory>
#include <span>
#include <concepts>

using EntityId = size_t;
using ComponentType = size_t;

struct IComponentArray
{
  virtual size_t componentSize() const = 0;
  virtual void remove(size_t index) = 0;
  virtual char* data() = 0;
  virtual size_t size() const = 0;
  virtual void allocate() = 0;

  virtual ~IComponentArray() = default;
};

using ComponentArrayPtr = std::unique_ptr<IComponentArray>;

template<typename T>
class ComponentArray : public IComponentArray
{
  static_assert(std::is_default_constructible_v<T>);
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(T::TypeId && (T::TypeId & (T::TypeId - 1)) == 0, "TypeId must be power of 2");

  friend class ComponentArrayGroup;

  public:
    size_t componentSize() const override
    {
      return sizeof(T);
    }

    char* data() override
    {
      return reinterpret_cast<char*>(m_data.data());
    }

    size_t size() const override
    {
      return m_data.size();
    }

    void allocate() override
    {
      m_data.resize(m_data.size() + 1);
    }

  private:
    std::vector<T> m_data;

    void remove(size_t index) override
    {
      std::swap(m_data[index], m_data[m_data.size() - 1]);
      m_data.pop_back();
    }
};

class ComponentArrayGroup
{
  friend class World;

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
    std::span<T> components()
    {
      auto span = constComponents<T>();
      return std::span<T>{const_cast<T*>(span.data()), span.size()};
    }

    template<typename T>
    std::span<const T> components() const
    {
      return constComponents<T>();
    }

    template<typename T>
    T& component(EntityId entityId)
    {
      return components<T>()[entityPosition(entityId)];
    }

    template<typename T>
    const T& component(EntityId entityId) const
    {
      return components<T>()[entityPosition(entityId)];
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

    template<typename T>
    std::span<const T> constComponents() const
    {
      auto i = m_componentData.find(T::TypeId);
      if (i == m_componentData.end()) {
        throw std::runtime_error("Group does not have components of given type");
      }

      auto& array = i->second;
      return std::span<const T>{reinterpret_cast<const T*>(array->data()), array->size()};
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
};

using Archetype = size_t;

class World
{
  using GroupMap = std::map<Archetype, ComponentArrayGroup>;

  public:
    template<bool IsConstView>
    class View
    {
      using GroupMapRef = std::conditional<IsConstView, const GroupMap&, GroupMap&>::type;

      public:
        template<bool IsConstIter>
        class IteratorImpl
        {
          public:
            IteratorImpl(GroupMap& groups, GroupMap::iterator i, Archetype mask)
              : m_groups(groups)
              , m_i(i)
              , m_mask(mask)
            {
              while (m_i != m_groups.end() && (m_i->first & m_mask) != m_mask) {
                ++m_i;
              }
            }

            IteratorImpl(const GroupMap& groups, GroupMap::const_iterator i, Archetype mask)
              : m_groups(groups)
              , m_i(i)
              , m_mask(mask)
            {}

            ComponentArrayGroup& operator*() requires(!IsConstIter)
            {
              return m_i->second;
            }

            const ComponentArrayGroup& operator*() const
            {
              return m_i->second;
            }

            IteratorImpl<IsConstIter>& operator++()
            {
              ++m_i;
              while (m_i != m_groups.end() && (m_i->first & m_mask) != m_mask) {
                ++m_i;
              }
              return *this;
            }

            bool operator==(const IteratorImpl<IsConstIter>& rhs) const
            {
              return m_i == rhs.m_i;
            }

          private:
            std::conditional<IsConstIter, const GroupMap&, GroupMap&>::type& m_groups;
            std::conditional<IsConstIter, GroupMap::const_iterator, GroupMap::iterator>::type m_i;
            Archetype m_mask;
        };

        using iterator = IteratorImpl<false>;
        using const_iterator = IteratorImpl<true>;

        View(GroupMapRef& groups, Archetype mask)
          : m_groups(groups)
          , m_mask(mask)
        {}

        const_iterator cbegin()
        {
          return const_iterator{m_groups, m_groups.cbegin(), m_mask};
        }

        const_iterator cend()
        {
          return const_iterator{m_groups, m_groups.cend(), m_mask};
        }

        iterator begin() requires(!IsConstView)
        {
          return iterator{m_groups, m_groups.begin(), m_mask};
        }

        iterator end() requires(!IsConstView)
        {
          return iterator{m_groups, m_groups.end(), m_mask};
        }

      private:
        GroupMapRef m_groups;
        Archetype m_mask;
    };

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
    View<false> components()
    {
      Archetype mask = (Ts::TypeId | ...);
      return View<false>{m_groups, mask};
    }

    template<typename... Ts>
    View<true> components() const
    {
      Archetype mask = (Ts::TypeId | ...);
      return View<true>{m_groups, mask};
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

    template<typename T>
    const T& component(EntityId entityId) const
    {
      return component<T>(entityId);
    }

    bool hasEntity(EntityId entityId) const
    {
      return m_archetypes.contains(entityId);
    }

    template<typename T>
    bool hasComponentForEntity(EntityId entityId) const
    {
      auto i = m_archetypes.find(entityId);
      if (i == m_archetypes.end()) {
        return false;
      }
      m_groups.at(i->second).hasEntity(entityId);
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
    GroupMap m_groups;
    std::map<EntityId, Archetype> m_archetypes;
};
