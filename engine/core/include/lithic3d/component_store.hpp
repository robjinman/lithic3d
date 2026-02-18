// Very simple ECS. Entities are grouped by archetypes so components within an archetype can be
// stored contiguously for maximum cache efficiency.

#pragma once

#include "entity_id.hpp"
#include "exception.hpp"
#include <vector>
#include <map>
#include <cassert>
#include <stdexcept>
#include <span>
#include <cstring>
#include <concepts>

namespace lithic3d
{

using ComponentTypeId = uint64_t;

struct ComponentSpec
{
  ComponentTypeId id;
  size_t size;
  size_t alignment;
};

template<typename T>
concept ComponentType = requires {
  std::integral_constant<ComponentTypeId, T::TypeId>{};
};

template<typename... Ts>
requires (ComponentType<Ts> && ...)
struct type_list {};

template<typename>
struct is_type_list : std::false_type {};

template<typename... Ts>
requires (ComponentType<Ts> && ...)
struct is_type_list<type_list<Ts...>> : std::true_type {};

template<typename T> constexpr bool is_type_list_v = is_type_list<T>::value;

template<typename T>
concept TypeList = is_type_list<T>::value;

template<typename List1, typename List2>
requires TypeList<List1> && TypeList<List2>
struct concat_2;

template<typename... Ts, typename... Us>
requires (ComponentType<Ts> && ...) && (ComponentType<Us> && ...)
struct concat_2<type_list<Ts...>, type_list<Us...>>
{
  using type = type_list<Ts..., Us...>;
};

template<typename... Lists>
requires (TypeList<Lists> && ...)
struct concat_n;

template<typename L>
requires TypeList<L>
struct concat_n<L>
{
  using type = L;
};

template<typename L1, typename L2, typename... Rest>
requires TypeList<L1> && TypeList<L2> && (TypeList<Rest> && ...)
struct concat_n<L1, L2, Rest...>
{
  using type = typename concat_n<typename concat_2<L1, L2>::type, Rest...>::type;
};

template<typename T>
concept DeclaresRequiredComponents = requires { typename T::RequiredComponents; }
  && is_type_list_v<typename T::RequiredComponents>;

template<typename... Ts>
requires (DeclaresRequiredComponents<Ts> && ...)
using extract_components = typename concat_n<typename Ts::RequiredComponents...>::type;

template<typename... Ts>
requires (ComponentType<Ts> && ...)
void extractSpecs(std::vector<ComponentSpec>& specs, type_list<Ts...>)
{
  (specs.push_back(ComponentSpec{
    .id = Ts::TypeId,
    .size = sizeof(Ts),
    .alignment = alignof(Ts)
  }), ...);
}

template<typename T>
requires DeclaresRequiredComponents<T>
void extractSpecs(std::vector<ComponentSpec>& specs)
{
  extractSpecs(specs, typename T::RequiredComponents{});
}

class ComponentArray
{
  friend class ComponentArrayGroup;

  public:
    ComponentArray(size_t componentSize, size_t alignment)
      : m_componentSize(componentSize)
      , m_alignment(alignment)
    {
      m_data.reserve(1000);
    }

    size_t componentSize() const
    {
      return m_componentSize;
    }

    char* data()
    {
      return reinterpret_cast<char*>(m_alignedPtr);
    }

    size_t size() const
    {
      return m_numElements;
    }

  private:
    std::vector<char> m_data;
    void* m_alignedPtr = nullptr;
    size_t m_numElements = 0;
    size_t m_componentSize;
    size_t m_alignment;

    void resize(size_t size)
    {
      size_t n = size * m_componentSize + m_alignment - 1;
      m_data.resize(n);
      void* raw = m_data.data();
      m_alignedPtr = std::align(m_alignment, m_componentSize, raw, n);

      m_numElements = size;
    }

    void remove(size_t index)
    {
      size_t elementByteOffset = index * m_componentSize;
      char* data = reinterpret_cast<char*>(m_data.data());

      size_t numElements = size();

      size_t lastIndex = numElements - 1;
      size_t lastElementByteOffset = lastIndex * m_componentSize;

      memcpy(data + elementByteOffset, data + lastElementByteOffset, m_componentSize);

      resize(numElements - 1);
    }
};

using ComponentArrayPtr = std::unique_ptr<ComponentArray>;

class ComponentArrayGroup
{
  friend class ComponentStore;

  public:
    template<typename... Ts>  // Beware, we might have duplicate types
    requires (ComponentType<Ts> && ...)
    void allocate(EntityId entityId)
    {
      m_entityIds.push_back(entityId);
      m_indices.insert({ entityId, m_entityIds.size() - 1 });
      (allocate(Ts::TypeId, sizeof(Ts), alignof(Ts)), ...);
    }

    // Dynamic version
    void allocate(EntityId entityId, const std::vector<ComponentSpec>& specs)
    {
      m_entityIds.push_back(entityId);
      m_indices.insert({ entityId, m_entityIds.size() - 1 });

      for (auto& spec : specs) {
        allocate(spec.id, spec.size, spec.alignment);
      }
    }

    void allocate(ComponentTypeId typeId, size_t componentSize, size_t alignment)
    {
      auto i = m_componentData.find(typeId);
      if (i == m_componentData.end()) {
        auto array = std::make_unique<ComponentArray>(componentSize, alignment);
        i = m_componentData.insert({ typeId, std::move(array) }).first;
      }

      i->second->resize(m_entityIds.size());

      assert(m_entityIds.size() == m_indices.size());
    }

    template<typename T>
    requires ComponentType<T>
    std::span<T> components()
    {
      auto span = constComponents<T>();
      return std::span<T>{const_cast<T*>(span.data()), span.size()};
    }

    template<typename T>
    requires ComponentType<T>
    std::span<const T> components() const
    {
      return constComponents<T>();
    }

    template<typename T>
    requires ComponentType<T>
    T& component(EntityId entityId)
    {
      return components<T>()[entityPosition(entityId)];
    }

    template<typename T>
    requires ComponentType<T>
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
    std::map<ComponentTypeId, ComponentArrayPtr> m_componentData;
    std::vector<EntityId> m_entityIds;
    std::map<EntityId, size_t> m_indices;

    template<typename T>
    requires ComponentType<T>
    std::span<const T> constComponents() const
    {
      auto i = m_componentData.find(T::TypeId);
      if (i == m_componentData.end()) {
        return std::span<const T>{};
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

class ComponentStore
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
              skipNonMatchingGroups();
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
              skipNonMatchingGroups();
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

            void skipNonMatchingGroups()
            {
              while (m_i != m_groups.end() && (m_i->first & m_mask) != m_mask) {
                ++m_i;
              }
            }
        };

        using iterator = IteratorImpl<false>;
        using const_iterator = IteratorImpl<true>;

        View(GroupMapRef& groups, Archetype mask)
          : m_groups(groups)
          , m_mask(mask)
        {}

        const_iterator cbegin() const
        {
          return const_iterator{m_groups, m_groups.cbegin(), m_mask};
        }

        const_iterator cend() const
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
    requires (ComponentType<Ts> && ...)
    void allocateEntity(EntityId entityId)
    {
      Archetype archetype = (Ts::TypeId | ...);

      ASSERT(!m_archetypes.contains(entityId), "Entity ID " << entityId << " already taken");

      m_groups[archetype].allocate<Ts...>(entityId);
      m_archetypes[entityId] = archetype;
    }

    template<typename... Ts>
    requires (ComponentType<Ts> && ...)
    void allocateEntity_(EntityId entityId, type_list<Ts...>)
    {
      allocateEntity<Ts...>(entityId);
    }

    template<typename... Ts>
    requires (DeclaresRequiredComponents<Ts> && ...)
    void allocate(EntityId entityId)
    {
      allocateEntity_(entityId, extract_components<Ts...>{});
    }

    // Dynamic version
    void allocateEntity(EntityId entityId, const std::vector<ComponentSpec>& specs)
    {
      Archetype archetype = 0;
      for (auto& spec : specs) {
        archetype |= spec.id;
      }

      ASSERT(!m_archetypes.contains(entityId), "Entity ID " << entityId << " already taken");

      m_groups[archetype].allocate(entityId, specs);
      m_archetypes[entityId] = archetype;
    }

    template<typename T>
    requires ComponentType<T>
    T& instantiate(EntityId entityId)
    {
      auto addr = &component<T>(entityId);
      return *std::launder(new (addr) T{});
    }

    template<typename... Ts>
    requires (ComponentType<Ts> && ...)
    View<false> components()
    {
      Archetype mask = (Ts::TypeId | ...);
      return View<false>{m_groups, mask};
    }

    template<typename... Ts>
    requires (ComponentType<Ts> && ...)
    View<true> components() const
    {
      Archetype mask = (Ts::TypeId | ...);
      return View<true>{m_groups, mask};
    }

    template<typename T>
    requires ComponentType<T>
    T& component(EntityId entityId)
    {
      auto i = m_archetypes.find(entityId);
      if (i == m_archetypes.end()) {
        throw std::runtime_error("No such entity");
      }
      return m_groups.at(i->second).component<T>(entityId);
    }

    template<typename T>
    requires ComponentType<T>
    const T& component(EntityId entityId) const
    {
      return (const_cast<ComponentStore*>(this)->component<T>(entityId));
    }

    bool hasEntity(EntityId entityId) const
    {
      return m_archetypes.contains(entityId);
    }

    ComponentArrayGroup& group(EntityId entityId)
    {
      return m_groups.at(m_archetypes.at(entityId));
    }

    const ComponentArrayGroup& group(EntityId entityId) const
    {
      return m_groups.at(m_archetypes.at(entityId));
    }

    template<typename T>
    requires ComponentType<T>
    bool hasComponentForEntity(EntityId entityId) const
    {
      auto i = m_archetypes.find(entityId);
      if (i == m_archetypes.end()) {
        return false;
      }
      return i->second & T::TypeId;
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
    GroupMap m_groups;
    std::map<EntityId, Archetype> m_archetypes;
};

using ComponentStorePtr = std::unique_ptr<ComponentStore>;

} // namespace lithic3d
