// Implements a loose, square octree

#include "lithic3d/spatial_container.hpp"

namespace lithic3d
{
namespace {

constexpr float MIN_CELL_SIZE = 1.f;
constexpr float SQRT_3 = 1.73205080757f;

struct Cell
{
  Vec3f min;
  float size = 0.f;

  inline bool contains(const Vec3f& pos, float radius)
  {
    return pos[0] - radius >= min[0] && pos[0] + radius <= min[0] + size &&
      pos[1] - radius >= min[1] && pos[1] + radius <= min[1] + size &&
      pos[2] - radius >= min[2] && pos[2] + radius <= min[2] + size;
  }
};

struct Node
{
  Node* parent = nullptr;
  uint8_t index = 0; // 0 to 7
  Cell cell;
  uint8_t numChildren = 0;
  std::array<std::unique_ptr<Node>, 8> children;
  std::set<EntityId> objects;
};

class SpatialContainerImpl : public SpatialContainer
{
  public:
    SpatialContainerImpl();

    void insert(EntityId entityId, const Vec3f& pos, float radius) override;
    void move(EntityId entityId, const Vec3f& pos, float radius) override;
    void remove(EntityId entityId) override;
    std::set<EntityId> getIntersecting(const Frustum& volume) const override;

  private:
    std::unique_ptr<Node> m_root;
    std::unordered_map<EntityId, Node*> m_entities;

    void insert(Node& node, EntityId id, const Vec3f& pos, float radius);
    void collapseNode(Node& node);
    void getIntersecting(const Node& node, const Frustum& frustum,
      std::set<EntityId>& entities) const;
};

SpatialContainerImpl::SpatialContainerImpl()
{
  m_root = std::unique_ptr<Node>(new Node{
    .parent = nullptr,
    .index = 0,
    .cell = {
      .min{ -1000.f, -1000.f, -1000.f }, // TODO
      .size = 2000.f
    },
    .numChildren = 0,
    .children{},
    .objects{}
  });
}

inline Cell createChildCell(const Cell& parentCell, uint8_t n)
{
  assert(n < 8);
  auto d = parentCell.size * 0.5f;

  return Cell{
    .min = parentCell.min + Vec3f{
      n & 0b001 ? d : 0,
      n & 0b010 ? d : 0,
      n & 0b100 ? d : 0
    },
    .size = d
  };
}

void SpatialContainerImpl::insert(Node& node, EntityId entityId, const Vec3f& pos, float radius)
{
  uint8_t childIndex = std::numeric_limits<uint8_t>::max();

  if (node.cell.size * 0.5f >= MIN_CELL_SIZE) {
    for (size_t i = 0; i < 8; ++i) {
      auto childCell = createChildCell(node.cell, i);

      if (childCell.contains(pos, radius)) {
        childIndex = i;
      }
    }
  }

  if (childIndex < 8) {
    // Lazily create child
    if (node.children[childIndex] == nullptr) {
      node.children[childIndex] = std::unique_ptr<Node>(new Node{
        .parent = &node,
        .index = childIndex,
        .cell = createChildCell(node.cell, childIndex),
        .numChildren = 0,
        .children{},
        .objects{}
      });

      ++node.numChildren;
    }

    insert(*node.children[childIndex], entityId, pos, radius);
  }
  else {
    node.objects.insert(entityId);
    m_entities[entityId] = &node;
  }
}

void SpatialContainerImpl::insert(EntityId entityId, const Vec3f& pos, float radius)
{
  insert(*m_root, entityId, pos, radius);
}

void SpatialContainerImpl::move(EntityId entityId, const Vec3f& pos, float radius)
{
  // For now just remove and re-insert
  // TODO: Optimise

  remove(entityId);
  insert(entityId, pos, radius);
}

void SpatialContainerImpl::collapseNode(Node& node)
{
  assert(node.numChildren == 0);
  assert(node.objects.empty());

  Node* parent = node.parent;

  if (parent != nullptr) {
    parent->children[node.index].reset(); // Deletes node
    --parent->numChildren;
    if (parent->numChildren == 0 && parent->objects.empty()) {
      collapseNode(*parent);
    }
  }
}

void SpatialContainerImpl::remove(EntityId entityId)
{
  auto i = m_entities.find(entityId);
  if (i != m_entities.end()) {
    auto& objects = i->second->objects;
    for (auto j = objects.begin(); j != objects.end(); ++j) {
      if (*j == entityId) {
        objects.erase(j);

        if (objects.empty() && i->second->numChildren == 0) {
          collapseNode(*i->second);
        }
        return;
      }
    }
    m_entities.erase(i);
  }
}

bool intersectsFrustum(const Frustum& frustum, const Vec3f& pos, float radius)
{
  for (auto& plane : frustum) {
    float dist = plane.normal.dot(pos) + plane.distance;
    if (dist < -radius) {
      return false;
    }
  }

  return true;
}

bool intersectsFrustum(const Node& node, const Frustum& frustum)
{
  auto& cell = node.cell;
  auto pos = cell.min + Vec3f{ cell.size, cell.size, cell.size } * 0.5f;
  float radius = cell.size * SQRT_3;

  return intersectsFrustum(frustum, pos, radius);
}

void SpatialContainerImpl::getIntersecting(const Node& node, const Frustum& frustum,
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

std::set<EntityId> SpatialContainerImpl::getIntersecting(const Frustum& frustum) const
{
  std::set<EntityId> entities;

  getIntersecting(*m_root, frustum, entities);

  return entities;
}

} // namespace

SpatialContainerPtr createSpatialContainer()
{
  return std::make_unique<SpatialContainerImpl>();
}

}
