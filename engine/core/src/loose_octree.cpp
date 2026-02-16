#include "lithic3d/loose_octree.hpp"
#include <array>

namespace lithic3d
{
namespace {

constexpr float MIN_CELL_SIZE = 1.f;
constexpr float SQRT_3 = 1.73205080757f;

inline bool contains(const OctreeCell& cell, const Vec3f& pos)
{
  return
    pos[0] >= cell.min[0] && pos[0] <= cell.min[0] + cell.size &&
    pos[1] >= cell.min[1] && pos[1] <= cell.min[1] + cell.size &&
    pos[2] >= cell.min[2] && pos[2] <= cell.min[2] + cell.size;
}

class LooseOctreeImpl : public LooseOctree
{
  public:
    LooseOctreeImpl(const Vec3f& min, float size);

    void insert(EntityId entityId, const Vec3f& pos, float radius) override;
    void move(EntityId entityId, const Vec3f& pos, float radius) override;
    void remove(EntityId entityId) override;
    std::set<EntityId> getIntersecting(const Frustum& volume) const override;

    const OctreeNode& test_root() const override;

  private:
    std::unique_ptr<OctreeNode> m_root;
    std::unordered_map<EntityId, OctreeNode*> m_entities;

    void insert(OctreeNode& node, EntityId id, const Vec3f& pos, float radius);
    void collapseOctreeNode(OctreeNode& node);
    void getIntersecting(const OctreeNode& node, const Frustum& frustum,
      std::set<EntityId>& entities) const;
};

LooseOctreeImpl::LooseOctreeImpl(const Vec3f& min, float size)
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
    .objects{}
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
  return OctreeCell{
    .min = cell.min - Vec3f{ cell.size, cell.size, cell.size } * 0.5f,
    .size = cell.size * 2.f
  };
}

void LooseOctreeImpl::insert(OctreeNode& node, EntityId entityId, const Vec3f& pos, float radius)
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
        .objects{}
      });

      ++node.numChildren;
    }

    insert(*node.children[childIndex], entityId, pos, radius);
  }
  else {
    // Otherwise, just insert the item at this level

    node.objects.insert(entityId);
    m_entities[entityId] = &node;
  }
}

void LooseOctreeImpl::insert(EntityId entityId, const Vec3f& pos, float radius)
{
  insert(*m_root, entityId, pos, radius);
}

void LooseOctreeImpl::move(EntityId entityId, const Vec3f& pos, float radius)
{
  // For now just remove and re-insert
  // TODO: Optimise

  remove(entityId);
  insert(entityId, pos, radius);
}

void LooseOctreeImpl::collapseOctreeNode(OctreeNode& node)
{
  assert(node.numChildren == 0);
  assert(node.objects.empty());

  OctreeNode* parent = node.parent;

  if (parent != nullptr) {
    parent->children[node.index].reset(); // Deletes node
    --parent->numChildren;
    if (parent->numChildren == 0 && parent->objects.empty()) {
      collapseOctreeNode(*parent);
    }
  }
}

void LooseOctreeImpl::remove(EntityId entityId)
{
  auto i = m_entities.find(entityId);
  if (i != m_entities.end()) {
    auto& objects = i->second->objects;
    objects.erase(entityId);

    if (objects.empty() && i->second->numChildren == 0) {
      collapseOctreeNode(*i->second);
    }

    m_entities.erase(i);
  }
}

bool intersectsFrustum(const OctreeNode& node, const Frustum& frustum)
{
  auto& cell = node.looseBounds;
  auto pos = cell.min + Vec3f{ cell.size, cell.size, cell.size } * 0.5f;
  float radius = 0.5f * cell.size * SQRT_3;

  return intersectsFrustum(frustum, pos, radius);
}

void LooseOctreeImpl::getIntersecting(const OctreeNode& node, const Frustum& frustum,
  std::set<EntityId>& entities) const
{
  entities.insert(node.objects.cbegin(), node.objects.cend());

  for (auto& child : node.children) {
    if (child == nullptr) {
      continue;
    }

    if (intersectsFrustum(*child, frustum)) {
      getIntersecting(*child, frustum, entities);
    }
  }
}

std::set<EntityId> LooseOctreeImpl::getIntersecting(const Frustum& frustum) const
{
  std::set<EntityId> entities;

  getIntersecting(*m_root, frustum, entities);

  return entities;
}

const OctreeNode& LooseOctreeImpl::test_root() const
{
  return *m_root;
}

} // namespace

LooseOctreePtr createLooseOctree(const Vec3f& min, float size)
{
  return std::make_unique<LooseOctreeImpl>(min, size);
}

}
