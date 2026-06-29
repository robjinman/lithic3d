#pragma once

#include "component_store.hpp"
#include "math.hpp"
#include <memory>
#include <array>
#include <unordered_map>

namespace lithic3d
{

template<typename T_KEY, typename T_VAL>
class LooseOctree
{
  public:
    struct Entry
    {
      T_KEY key;
      T_VAL value;
    };

  private:
    struct OctreeCell
    {
      Vec3f min;
      float size = 0.f;
    };

    struct OctreeNode
    {
      OctreeNode* parent = nullptr;
      uint8_t index = 0; // 0 to 7
      OctreeCell tightBounds;
      OctreeCell looseBounds;
      uint8_t numChildren = 0;
      std::array<std::unique_ptr<OctreeNode>, 8> children;
      std::vector<Entry> items;
    };

  public:
    LooseOctree(const Vec3f& min, float size)
    {
      m_root = std::unique_ptr<OctreeNode>(new OctreeNode{
        .parent = nullptr,
        .index = 0,
        .tightBounds = {
          .min = min,
          .size = size
        },
        .looseBounds = {
          .min = min - Vec3f{ size, size, size } * 0.5f,
          .size = size * 2.f
        },
        .numChildren = 0,
        .children{},
        .items{}
      });
    }

    inline OctreeCell createChildCell(const OctreeCell& parentOctreeCell, uint8_t n)
    {
      assert(n < 8);
      auto d = parentOctreeCell.size * 0.5f;

      return OctreeCell{
        .min = parentOctreeCell.min + Vec3f{
          n & 0b001 ? d : 0,
          n & 0b010 ? d : 0,
          n & 0b100 ? d : 0
        },
        .size = d
      };
    }

    inline OctreeCell createLooseBounds(const OctreeCell& cell)
    {
      return {
        .min = cell.min - Vec3f{ cell.size, cell.size, cell.size } * 0.5f,
        .size = cell.size * 2.f
      };
    }

    void insert(T_KEY key, const T_VAL& value, const Vec3f& pos, float radius)
    {
      insert(*m_root, key, value, pos, radius);
    }

    void move(T_KEY key, const Vec3f& pos, float radius)
    {
      // For now just remove and re-insert
      // TODO: Optimise

      auto& location = m_locations.at(key);
      auto value = location.node->items[location.index].value;

      remove(key);
      insert(key, value, pos, radius);
    }

    void remove(T_KEY key)
    {
      auto i = m_locations.find(key);
      if (i != m_locations.end()) {
        auto& items = i->second.node->items;

        auto index = i->second.index;
        items.erase(items.begin() + index);

        for (auto j = index; j < items.size(); ++j) {
          --m_locations.at(items[j].key).index;
        }

        if (items.empty() && i->second.node->numChildren == 0) {
          collapseOctreeNode(*i->second.node);
        }

        m_locations.erase(i);
      }
    }

    T_VAL& item(T_KEY key)
    {
      auto location = m_locations.at(key);
      return location.node->items[location.index].value;
    }

    bool intersectsFrustum(const OctreeNode& node, const Frustum& frustum) const
    {
      auto& cell = node.looseBounds;
      auto pos = cell.min + Vec3f{ cell.size, cell.size, cell.size } * 0.5f;
      float radius = 0.5f * cell.size * SQRT_3;

      return lithic3d::intersectsFrustum(frustum, pos, radius);
    }

    std::vector<Entry> getIntersecting(const Frustum& frustum) const
    {
      std::vector<Entry> items;

      getIntersecting(*m_root, frustum, items);

      return items;
    }

    std::vector<Entry> getIntersecting(const Vec3f&, const Vec3f&) const
    {
      // TODO: Do this properly

      std::vector<Entry> items;

      for (auto& [ key, location ] : m_locations) {
        items.push_back({ key, location.node->items[location.index].value });
      }

      return items;
    }

      // Exposed for testing
    const OctreeNode& test_root() const
    {
      return *m_root;
    }

    virtual ~LooseOctree() = default;

  private:
    static constexpr float MIN_CELL_SIZE = 1.f;
    static constexpr float SQRT_3 = 1.73205080757f;

    struct ItemLocation
    {
      OctreeNode* node = nullptr;
      uint32_t index = 0;
    };

    std::unique_ptr<OctreeNode> m_root;
    std::unordered_map<T_KEY, ItemLocation> m_locations;

    inline bool contains(const OctreeCell& cell, const Vec3f& pos)
    {
      return
        pos[0] >= cell.min[0] && pos[0] <= cell.min[0] + cell.size &&
        pos[1] >= cell.min[1] && pos[1] <= cell.min[1] + cell.size &&
        pos[2] >= cell.min[2] && pos[2] <= cell.min[2] + cell.size;
    }

    void getIntersecting(const OctreeNode& node, const Frustum& frustum,
      std::vector<Entry>& items) const
    {
      items.insert(items.end(), node.items.cbegin(), node.items.cend());

      for (auto& child : node.children) {
        if (child == nullptr) {
          continue;
        }

        if (intersectsFrustum(*child, frustum)) {
          getIntersecting(*child, frustum, items);
        }
      }
    }

    void collapseOctreeNode(OctreeNode& node)
    {
      assert(node.numChildren == 0);
      assert(node.items.empty());

      OctreeNode* parent = node.parent;

      if (parent != nullptr) {
        parent->children[node.index].reset(); // Deletes node
        --parent->numChildren;
        if (parent->numChildren == 0 && parent->items.empty()) {
          collapseOctreeNode(*parent);
        }
      }
    }

    void insert(OctreeNode& node, T_KEY key, const T_VAL& value, const Vec3f& pos, float radius)
    {
      const float diameter = radius * 2.f;
      const float childCellSize = node.tightBounds.size * 0.5f;

      uint8_t childIndex = std::numeric_limits<uint8_t>::max();

      // If this cell can be split further and the item would fit into a child cell
      if (childCellSize >= MIN_CELL_SIZE && diameter <= childCellSize) { 
        for (size_t i = 0; i < 8; ++i) {
          auto bounds = createChildCell(node.tightBounds, i);

          if (contains(bounds, pos)) {
            childIndex = i;
            break;
          }
        }
      }

      if (childIndex < 8) {
        // Lazily create child
        if (node.children[childIndex] == nullptr) {
          auto tightBounds = createChildCell(node.tightBounds, childIndex);

          node.children[childIndex] = std::unique_ptr<OctreeNode>(new OctreeNode{
            .parent = &node,
            .index = childIndex,
            .tightBounds = tightBounds,
            .looseBounds = createLooseBounds(tightBounds),
            .numChildren = 0,
            .children{},
            .items{}
          });

          ++node.numChildren;
        }

        insert(*node.children[childIndex], key, value, pos, radius);
      }
      else {
        // Otherwise, just insert the item at this level

        uint32_t i = node.items.size();
        node.items.push_back({ key, value });
        m_locations[key] = { &node, i };
      }
    }
};

template<typename T_KEY, typename T_VAL>
using LooseOctreePtr = std::unique_ptr<LooseOctree<T_KEY, T_VAL>>;

}
