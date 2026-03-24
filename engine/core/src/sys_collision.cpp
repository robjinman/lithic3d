#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/input.hpp"
#include <iostream> // TODO

namespace lithic3d
{
namespace
{

const float G = -metresToWorldUnits(9.8f) / (TICKS_PER_SECOND * TICKS_PER_SECOND);

uint32_t findFreeIndex(const std::array<Force, MAX_FORCES>& array)
{
  for (size_t i = 0; i < MAX_FORCES; ++i) {
    if (array[i].lifetime == 0) {
      return i;
    }
  }
  return MAX_FORCES;
}

// Returns the matrix that transforms FROM the new space
Mat3x3f changeOfBasisMatrix(const Vec3f& yAxis, const Vec3f& other)
{
  Vec3f xAxis = yAxis.cross(other).normalise();
  Vec3f zAxis = yAxis.cross(xAxis).normalise();

  return {
    xAxis[0], yAxis[0], zAxis[0],
    xAxis[1], yAxis[1], zAxis[1],
    xAxis[2], yAxis[2], zAxis[2]
  };
}

Mat3x3f skewSymmetricMatrix(const Vec3f& v)
{
  return {
    0.f, -v[2], v[1],
    v[2], 0.f, -v[0],
    -v[1], v[0], 0.f
  };
}

Mat3x3f computeInverseInertialTensor(const BoundingBox& box, float inverseMass)
{
  float w = box.max[0] - box.min[0];
  float h = box.max[1] - box.min[1];
  float d = box.max[2] - box.min[2];

  return {
    12.f * inverseMass / (h * h + d * d), 0.f, 0.f,
    0.f, 12.f * inverseMass / (w * w + d * d), 0.f,
    0.f, 0.f, 12.f * inverseMass / (w * w + h * h)
  };
}

// TODO: Rationalise this
Mat3x3f transformTensor(const Mat3x3f& I, const Mat4x4f& m)
{
  // Normalise to remove scale
  Vec3f i = Vec3f{ m.at(0, 0), m.at(1, 0), m.at(2, 0) }.normalise();
  Vec3f j = Vec3f{ m.at(0, 1), m.at(1, 1), m.at(2, 1) }.normalise();
  Vec3f k = Vec3f{ m.at(0, 2), m.at(1, 2), m.at(2, 2) }.normalise();

  Mat3x3f R{
    i[0], j[0], k[0],
    i[1], j[1], k[1],
    i[2], j[2], k[2]
  };

  Mat3x3f invR{
    i[0], i[1], i[2],
    j[0], j[1], j[2],
    k[0], k[1], k[2]
  };

  return R * I * invR;
}

struct ObjectComponents
{
  EntityId entityId = 0;
  CLocalTransform* localTransform = nullptr;
  CSpatialFlags* spatialFlags = nullptr;
  CCollision* collision = nullptr;
  CCollisionDynamic* dynamic = nullptr;
  CCollisionBox* box = nullptr;
  CCollisionCapsule* capsule = nullptr;
  CCollisionTerrain* terrain = nullptr;
};

struct CollisionPair
{
  ObjectComponents A;
  ObjectComponents B;
};

struct Contact
{
  ObjectComponents A;
  ObjectComponents B;
  Vec3f point;
  Vec3f normal;
  float penetration = 0.f;
  Mat3x3f toContactSpace;
  Mat3x3f fromContactSpace;
};

struct Edge
{
  Vec3f A;
  Vec3f B;
};

class HeightMapSampler
{
  public:
    HeightMapSampler(const HeightMap& heightMap, const Vec3f& position)
      : m_map(heightMap)
      , m_pos(position) {}

    std::array<Vec3f, 3> triangle(const Vec2f& p) const;

  private:
    const HeightMap m_map;
    Vec3f m_pos;
};

std::array<Vec3f, 3> HeightMapSampler::triangle(const Vec2f& p) const
{
  DBG_ASSERT(inRange(p[0], m_pos[0], m_pos[0] + m_map.width), "Value out of range");
  DBG_ASSERT(inRange(p[1], m_pos[1], m_pos[1] + m_map.height), "Value out of range");

  float xIdx = (p[0] - m_pos[0]) * m_map.widthPx / m_map.width;
  float zIdx = (p[1] - m_pos[1]) * m_map.heightPx / m_map.height;

  float dx = m_map.width / (m_map.widthPx - 1);
  float dz = m_map.height / (m_map.heightPx - 1);

  auto xIdx0 = floorf(xIdx);
  auto zIdx0 = floorf(zIdx);
  auto xIdx1 = ceilf(xIdx);
  auto zIdx1 = ceilf(zIdx);

  auto getVertex = [this, dx, dz](float xIdx, float zIdx) {
    size_t i = static_cast<size_t>(zIdx) * m_map.widthPx + static_cast<size_t>(xIdx);
    return Vec3f{ dx * xIdx, m_map.data.at(i), dz * zIdx };
  };

  Vec3f A = getVertex(xIdx0, zIdx1);
  Vec3f B = getVertex(xIdx1, zIdx1);
  Vec3f C = getVertex(xIdx1, zIdx0);
  Vec3f D = getVertex(xIdx0, zIdx0);

  Vec2f idx = Vec2f{ xIdx, zIdx };
  float distanceFromD = (idx - Vec2f{ xIdx0, zIdx0 }).squareMagnitude();
  float distanceFromB = (idx - Vec2f{ xIdx1, zIdx1 }).squareMagnitude();

  return distanceFromD < distanceFromB ?
    std::array<Vec3f, 3>{ A, C, D } :
    std::array<Vec3f, 3>{ A, B, C };
}

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DStaticBox& data) override;
    void addEntity(EntityId id, const DDynamicBox& data) override;
    void addEntity(EntityId id, const DTerrainChunk& data) override;

    void setInverseMass(EntityId id, float inverseMass) override;
    void applyForce(EntityId id, const Vec3f& force, float seconds) override;
    void applyTorque(EntityId id, const Vec3f& torque, float seconds) override;
    void setStationary(EntityId id) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    Ecs& m_ecs;

    void applyForce(CCollisionDynamic& comp, const Force& force);
    void applyTorque(CCollisionDynamic& comp, const Force& torque);
    void applyGravity(CCollisionDynamic& comp);
    std::vector<CollisionPair> findPossibleCollisions();
    std::vector<Contact> generateContacts(const std::vector<CollisionPair>& pairs);
    void integrate();
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

void SysCollisionImpl::addEntity(EntityId id, const DStaticBox& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionBox>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction
  };

  componentStore.instantiate<CCollisionBox>(id) = CCollisionBox{
    .boundingBox = data.boundingBox
  };
}

void SysCollisionImpl::addEntity(EntityId id, const DDynamicBox& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionBox>(componentStore, id);
  assertHasComponent<CCollisionDynamic>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction
  };

  componentStore.instantiate<CCollisionBox>(id) = CCollisionBox{
    .boundingBox = data.boundingBox
  };

  CCollisionDynamic dynamic{
    .inverseMass = data.inverseMass,
    .centreOfMass = data.centreOfMass,
    .linearForces{},
    .linearAcceleration{},
    .linearVelocity{},
    .torques{},
    .angularAcceleration{},
    .angularVelocity{},
    .inverseInertialTensor = computeInverseInertialTensor(data.boundingBox, data.inverseMass)
  };

  if (dynamic.inverseMass != 0) {
    applyGravity(dynamic);
  }

  componentStore.instantiate<CCollisionDynamic>(id) = dynamic;
}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
  };
}

void SysCollisionImpl::removeEntity(EntityId entityId)
{
  // TODO
}

bool SysCollisionImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

void SysCollisionImpl::applyGravity(CCollisionDynamic& comp)
{
  if (comp.inverseMass == 0.f) {
    return;
  }

  float f = (1.f / comp.inverseMass) * G;

  applyForce(comp, {
    .force{ 0.f, f, 0.f },
    .lifetime = std::numeric_limits<uint32_t>::max()
  });
}

void SysCollisionImpl::applyForce(EntityId id, const Vec3f& force, float seconds)
{
  auto& comp = m_ecs.componentStore().component<CCollisionDynamic>(id);
  applyForce(comp, {
    .force = force,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyForce(CCollisionDynamic& comp, const Force& force)
{
  uint32_t i = findFreeIndex(comp.linearForces);
  if (i == MAX_FORCES) {
    m_logger.warn("Max forces exceeded on object");
    return;
  }
  comp.linearForces[i] = force;
  comp.idle = false;
  comp.framesIdle = 0;
}

void SysCollisionImpl::applyTorque(EntityId id, const Vec3f& torque, float seconds)
{
  auto& comp = m_ecs.componentStore().component<CCollisionDynamic>(id);
  applyTorque(comp, {
    .force = torque,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyTorque(CCollisionDynamic& comp, const Force& torque)
{
  uint32_t i = findFreeIndex(comp.torques);
  if (i == MAX_FORCES) {
    m_logger.warn("Max torques exceeded on object");
    return;
  }
  comp.torques[i] = torque;
  comp.idle = false;
  comp.framesIdle = 0;
}

void SysCollisionImpl::setStationary(EntityId id)
{
  auto& comp = m_ecs.componentStore().component<CCollisionDynamic>(id);
  comp.linearAcceleration = {};
  comp.linearVelocity = {};
  comp.linearForces = {};
  comp.angularAcceleration = {};
  comp.angularVelocity = {};
  comp.torques = {};

  if (comp.inverseMass != 0.f) {
    applyGravity(comp);
  }
}

void SysCollisionImpl::setInverseMass(EntityId id, float inverseMass)
{
  auto& dynamic = m_ecs.componentStore().component<CCollisionDynamic>(id);
  auto& box = m_ecs.componentStore().component<CCollisionBox>(id);

  bool shouldApplyGravity = dynamic.inverseMass == 0.f && inverseMass != 0.f;

  dynamic.inverseMass = inverseMass;
  dynamic.inverseInertialTensor = computeInverseInertialTensor(box.boundingBox,
    dynamic.inverseMass);

  if (shouldApplyGravity) {
    applyGravity(dynamic);
  }
}

std::vector<CollisionPair> SysCollisionImpl::findPossibleCollisions()
{
  // Extremely naive collision algorithm
  // TODO: Use something like sweep & prune instead: https://leanrada.com/notes/sweep-and-prune/

  std::vector<CollisionPair> pairs;

  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CLocalTransform, CBoundingBox, CCollision
  >();

  size_t outerIdx = 0;
  for (auto& group1 : groups) {
    auto flagsComps1 = group1.components<CSpatialFlags>();
    auto localTs1 = group1.components<CLocalTransform>();
    auto boundingBoxes1 = group1.components<CBoundingBox>();
    auto collComps1 = group1.components<CCollision>();
    auto collBoxComps1 = group1.components<CCollisionBox>();
    auto collDynamicComps1 = group1.components<CCollisionDynamic>();
    auto collTerrainComps1 = group1.components<CCollisionTerrain>();
    auto collCapsuleComps1 = group1.components<CCollisionCapsule>();
    auto entityIds1 = group1.entityIds();

    for (size_t i = 0; i < entityIds1.size(); ++i) {
      auto& flags = flagsComps1[i].flags;
      bool aIsInactive = collDynamicComps1.empty() || collDynamicComps1[i].idle;

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {

        size_t innerIdx = 0;
        for (auto& group2 : groups) {
          auto flagsComps2 = group2.components<CSpatialFlags>();
          auto localTs2 = group2.components<CLocalTransform>();
          auto boundingBoxes2 = group2.components<CBoundingBox>();
          auto collComps2 = group2.components<CCollision>();
          auto collBoxComps2 = group2.components<CCollisionBox>();
          auto collDynamicComps2 = group2.components<CCollisionDynamic>();
          auto collTerrainComps2 = group2.components<CCollisionTerrain>();
          auto collCapsuleComps2 = group2.components<CCollisionCapsule>();
          auto entityIds2 = group2.entityIds();

          for (size_t j = 0; j < entityIds2.size(); ++j) {
            if (innerIdx <= outerIdx) {
              ++innerIdx;
              continue;
            }

            auto& flagsJ = flagsComps2[j].flags;
            bool bIsInactive = collDynamicComps2.empty() || collDynamicComps2[j].idle;

            if (flagsJ.test(SpatialFlags::ParentEnabled) && flagsJ.test(SpatialFlags::Enabled)) {
              if (aIsInactive && bIsInactive) {
                ++innerIdx;
                continue;
              }

              auto& box1 = boundingBoxes1[i].worldSpaceAabb;
              auto& box2 = boundingBoxes2[j].worldSpaceAabb;

              if (box1.max[0] >= box2.min[0] && box1.min[0] <= box2.max[0] &&
                box1.max[1] >= box2.min[1] && box1.min[1] <= box2.max[1] &&
                box1.max[2] >= box2.min[2] && box1.min[2] <= box2.max[2]) {

                //std::cout << "Making pair: IDs(" << entityIds1[i] << ", " << entityIds2[j] << "). OuterIdx = " << outerIdx << ", innerIdx = " << innerIdx << "\n";

                pairs.push_back({
                  .A = {
                    .entityId = entityIds1[i],
                    .localTransform = &localTs1[i],
                    .spatialFlags = &flagsComps1[i],
                    .collision = &collComps1[i],
                    .dynamic = collDynamicComps1.empty() ? nullptr : &collDynamicComps1[i],
                    .box = collBoxComps1.empty() ? nullptr : &collBoxComps1[i],
                    .capsule = collCapsuleComps1.empty() ? nullptr : &collCapsuleComps1[i],
                    .terrain = collTerrainComps1.empty() ? nullptr : &collTerrainComps1[i]
                  },
                  .B = {
                    .entityId = entityIds2[j],
                    .localTransform = &localTs2[j],
                    .spatialFlags = &flagsComps2[j],
                    .collision = &collComps2[j],
                    .dynamic = collDynamicComps2.empty() ? nullptr : &collDynamicComps2[j],
                    .box = collBoxComps2.empty() ? nullptr : &collBoxComps2[j],
                    .capsule = collCapsuleComps2.empty() ? nullptr : &collCapsuleComps2[j],
                    .terrain = collTerrainComps2.empty() ? nullptr : &collTerrainComps2[j]
                  },
                });
              }
            }

            ++innerIdx;
          }
        }
      }

      ++outerIdx;
    }
  }

  return pairs;
}

inline bool isInside(const BoundingBox& box, const Vec3f& P, float epsilon = 0.f)
{
  return
    P[0] > box.min[0] - epsilon && P[0] < box.max[0] + epsilon &&
    P[1] > box.min[1] - epsilon && P[1] < box.max[1] + epsilon &&
    P[2] > box.min[2] - epsilon && P[2] < box.max[2] + epsilon;
}

// Returns box's vertices in world space
inline std::array<Vec3f, 8> getVertices(const BoundingBox& box, const Mat4x4f& transform)
{
  // C------D
  // |\     |\
  // | \    | \
  // A--\---B  \
  //  \  \   \  \
  //   \  G------H
  //    \ |    \ |
  //     \|     \|
  //      E------F
  return {
    (transform * box.transform * Vec4f{ box.min[0], box.min[1], box.min[2], 1.f }).sub<3>(),  // A  
    (transform * box.transform * Vec4f{ box.max[0], box.min[1], box.min[2], 1.f }).sub<3>(),  // B
    (transform * box.transform * Vec4f{ box.min[0], box.max[1], box.min[2], 1.f }).sub<3>(),  // C
    (transform * box.transform * Vec4f{ box.max[0], box.max[1], box.min[2], 1.f }).sub<3>(),  // D
    (transform * box.transform * Vec4f{ box.min[0], box.min[1], box.max[2], 1.f }).sub<3>(),  // E
    (transform * box.transform * Vec4f{ box.max[0], box.min[1], box.max[2], 1.f }).sub<3>(),  // F
    (transform * box.transform * Vec4f{ box.min[0], box.max[1], box.max[2], 1.f }).sub<3>(),  // G
    (transform * box.transform * Vec4f{ box.max[0], box.max[1], box.max[2], 1.f }).sub<3>()   // H
  };
}

// Returns box's inverse tangents at each vertex in world space
inline std::array<Vec3f, 8> getInverseTangents(const BoundingBox& box, const Mat4x4f& transform)
{
  // C------D
  // |\     |\
  // | \    | \
  // A--\---B  \
  //  \  \   \  \
  //   \  G------H
  //    \ |    \ |
  //     \|     \|
  //      E------F
  const float k = 0.57735026919; // One over root 3

  // TODO: Shouldn't need to normalise
  return {
    (transform * box.transform * Vec4f{ k, k, k, 0.f }).sub<3>().normalise(),     // A  
    (transform * box.transform * Vec4f{ -k, k, k, 0.f }).sub<3>().normalise(),    // B
    (transform * box.transform * Vec4f{ k, -k, k, 0.f }).sub<3>().normalise(),    // C
    (transform * box.transform * Vec4f{ -k, -k, k, 0.f }).sub<3>().normalise(),   // D
    (transform * box.transform * Vec4f{ k, k, -k, 0.f }).sub<3>().normalise(),    // E
    (transform * box.transform * Vec4f{ -k, k, -k, 0.f }).sub<3>().normalise(),   // F
    (transform * box.transform * Vec4f{ k, -k, -k, 0.f }).sub<3>().normalise(),   // G
    (transform * box.transform * Vec4f{ -k, -k, -k, 0.f }).sub<3>().normalise()   // H
  };
}

inline std::array<Edge, 12> getEdges(const std::array<Vec3f, 8>& vertices)
{
  return {
    Edge{ vertices[0], vertices[1] }, // AB
    Edge{ vertices[1], vertices[3] }, // BD
    Edge{ vertices[3], vertices[2] }, // DC
    Edge{ vertices[2], vertices[0] }, // CA
    Edge{ vertices[4], vertices[5] }, // EF
    Edge{ vertices[5], vertices[7] }, // FH
    Edge{ vertices[7], vertices[6] }, // HG
    Edge{ vertices[6], vertices[4] }, // GE
    Edge{ vertices[0], vertices[4] }, // AE
    Edge{ vertices[1], vertices[5] }, // BF
    Edge{ vertices[2], vertices[6] }, // CG
    Edge{ vertices[3], vertices[7] }, // DH
  };
}

// Returns penetration depth or 0 if there's no penetration
//
// P is the point in world space
// invTangent is the inverse tangent at P
float pointBoxPenetration(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const Mat4x4f& boxToWorldSpace, const Vec3f& P, const Vec3f& invTangent, Vec3f& normal)
{
  auto Q = (worldToBoxSpace * Vec4f{ P, { 1.f }}).sub<3>();
  auto V = (worldToBoxSpace * Vec4f{ invTangent, { 0.f }}).sub<3>();

  if (!isInside(box, Q)) {
    return 0.f;
  }

  float w = std::min(std::min(box.max[0] - box.min[0], box.max[1] - box.min[1]),
    box.max[2] - box.min[2]);

  float maxScore = std::numeric_limits<float>::lowest();
  int face = -1; // 0 min-x, 1 max-x, 2 min-y, 3 max-y, 4 min-z, 5 max-z
  float contactPenetration = 0.f;

  // min-x (left) face
  float alignment = V.dot({ -1.f, 0.f, 0.f });
  float penetration = Q[0] - box.min[0];
  float proximity = penetration / w;
  float score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 0;
    contactPenetration = penetration;
  }
  // max-x (right) face
  alignment = V.dot({ 1.f, 0.f, 0.f });
  penetration = box.max[0] - Q[0];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 1;
    contactPenetration = penetration;
  }
  // min-y (bottom) face
  alignment = V.dot({ 0.f, -1.f, 0.f });
  penetration = Q[1] - box.min[1];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 2;
    contactPenetration = penetration;
  }
  // max-y (top) face
  alignment = V.dot({ 0.f, 1.f, 0.f });
  penetration = box.max[1] - Q[1];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 3;
    contactPenetration = penetration;
  }
  // min-z (far) face
  alignment = V.dot({ 0.f, 0.f, -1.f });
  penetration = Q[2] - box.min[2];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 4;
    contactPenetration = penetration;
  }
  // max-z (near) face
  alignment = V.dot({ 0.f, 0.f, 1.f });
  penetration = box.max[2] - Q[2];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 5;
    contactPenetration = penetration;
  }

  // Set w components to zero to ignore translation
  switch (face) {
    case 0: // min-x (left) face
      normal = (boxToWorldSpace * Vec4f{ -1.f, 0.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 1: // max-x (right) face
      normal = (boxToWorldSpace * Vec4f{ 1.f, 0.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 2: // min-y (bottom) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, -1.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 3: // max-y (top) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, 1.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 4: // min-z (far) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, 0.f, -1.f, 0.f }).sub<3>().normalise();
      break;
    case 5: // max-z (near) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, 0.f, 1.f, 0.f }).sub<3>().normalise();
      break;
    default:
      return 0.f;
  }

  return contactPenetration;
}

inline Vec3f differentVector(const Vec3f& v)
{
  static auto r = rotationMatrix3x3({ 1.f, 2.f, 3.f });
  return r * v;
}

// Check if any of box1's vertices intersect box2 and generate a contact
bool checkForPointContacts(const BoundingBox& box1, const Mat4x4f& box1Transform,
  const BoundingBox& box2, const Mat4x4f& box2ToWorldSpace, const Mat4x4f& worldToBox2Space,
  Contact& contact)
{
  float maxPenetration = 0.f;
  int vertWithMaxPenetration = -1;
  std::array<Vec3f, 8> normals;

  auto verts = getVertices(box1, box1Transform);
  auto invTangents = getInverseTangents(box1, box1Transform);
  for (size_t i = 0; i < 8; ++i) {
    float penetration = pointBoxPenetration(box2, worldToBox2Space, box2ToWorldSpace, verts[i],
      invTangents[i], normals[i]);

    if (penetration > maxPenetration) {
      vertWithMaxPenetration = i;
      maxPenetration = penetration;
    }
  }

  if (vertWithMaxPenetration == -1) {
    return false;
  }

  contact.normal = normals[vertWithMaxPenetration];
  contact.penetration = maxPenetration;
  contact.point = verts[vertWithMaxPenetration];
  contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
  contact.toContactSpace = contact.fromContactSpace.t();

  return true;
}

// Solve system of equations for s and t:
// l1: As + Bt + C = 0
// l2: Ds + Et + F = 0
// Returns false if no solution (lines are parallel)
inline bool solve(const Vec3f& l1, const Vec3f& l2, Vec2f& st)
{
  float determinant = l1[0] * l2[1] - l1[1] * l2[0];
  if (determinant == 0.f) {
    return false;
  }
  st = {
    (l1[1] * l2[2] - l1[2] * l2[1]) / determinant,
    (l1[2] * l2[0] - l2[2] * l1[0]) / determinant
  };
  return true;
}

// Returns penetration depth or 0 if there's no penetration
float edgeBoxPenetration(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const Mat4x4f& boxToWorldSpace, const Edge& worldSpaceEdge, Vec3f& point, Vec3f& normal)
{
  Edge edge{
    .A = (worldToBoxSpace * Vec4f{ worldSpaceEdge.A, { 1.f }}).sub<3>(),
    .B = (worldToBoxSpace * Vec4f{ worldSpaceEdge.B, { 1.f }}).sub<3>()
  };

  if (
    (edge.A[0] < box.min[0] && edge.B[0] < box.min[0]) ||
    (edge.A[1] < box.min[1] && edge.B[1] < box.min[1]) ||
    (edge.A[2] < box.min[2] && edge.B[2] < box.min[2]) ||
    (edge.A[0] > box.max[0] && edge.B[0] > box.max[0]) ||
    (edge.A[1] > box.max[1] && edge.B[1] > box.max[1]) ||
    (edge.A[2] > box.max[2] && edge.B[2] > box.max[2])
  ) {
    return 0.f;
  }

  // Line: a + sv
  Vec3f a = worldSpaceEdge.A;
  Vec3f v = worldSpaceEdge.B - worldSpaceEdge.A;

  auto boxEdges = getEdges(getVertices(box, boxToWorldSpace));

  std::array<Vec3f, 12> normals;
  std::array<Vec3f, 12> points;
  int indexOfMinPenetration = -1;

  float minSqPenetration = std::numeric_limits<float>::max();

  //std::cout << "Box\n";
  for (auto& e : boxEdges) {
    //std::cout << "(" << e.A << "), (" << e.B << "), ";
  }
  //std::cout << "\n";

  for (size_t i = 0; i < 12; ++i) {
    auto& boxEdge = boxEdges[i];

    //std::cout << "Testing against edge" << i << ": (" << boxEdge.A << "), (" << boxEdge.B << ")\n";

    // Line: b + tw
    Vec3f b = boxEdge.A;
    Vec3f w = boxEdge.B - boxEdge.A;

    Vec2f st;
    if (!solve({
      -v.squareMagnitude(),
      v.dot(w),
      v.dot(b) - v.dot(a)
    },
    {
      -v.dot(w),
      w.squareMagnitude(),
      w.dot(b) - w.dot(a)
    }, st)) {
      //std::cout << "Failed to solve\n";
      continue;
    }

    //std::cout << "s = " << st[0] << "\n";
    //std::cout << "t = " << st[1] << "\n";

    if (!inRange(st[0], 0.f, 1.f)) {
      //std::cout << "s out of range\n";
      continue;
    }

    if (!inRange(st[1], 0.f, 1.f)) {
      //std::cout << "t out of range\n";
      continue;
    }

    Vec3f P = a + v * st[0];
    //std::cout << "P = (" << P << ")\n";

    Vec3f Q = b + w * st[1];
    //std::cout << "Q = (" << Q << ")\n";

    auto QP = Q - P;
    float sqPenetration = QP.squareMagnitude();

    //std::cout << "Penetration = " << QP.magnitude() << "\n";
    //std::cout << "sqPenetration = " << sqPenetration << "\n";

    if (!isInside(box, (worldToBoxSpace * Vec4f{ P[0], P[1], P[2], 1.f }).sub<3>(), 0.002f)) {
      //std::cout << "P not inside box\n";
      continue;
    }

    if (sqPenetration < minSqPenetration) {
      minSqPenetration = sqPenetration;
      indexOfMinPenetration = i;
      points[i] = P + QP * 0.5f;
      normals[i] = QP.normalise();
    }
  }

  if (indexOfMinPenetration != -1) {
    normal = normals[indexOfMinPenetration];
    point = points[indexOfMinPenetration];

    //std::cout << "Penetrating edge found! " << indexOfMinPenetration << "\n";
    //std::cout << "Penetration = " << sqrt(minSqPenetration) << "\n";

    return sqrt(minSqPenetration);
  }

  return 0.f;
}

// Check if any of box1's edges intersect box2 and generate a contact
bool checkForEdgeContacts(const BoundingBox& box1, const Mat4x4f& box1Transform,
  const BoundingBox& box2, const Mat4x4f& box2ToWorldSpace, const Mat4x4f& worldToBox2Space,
  Contact& contact)
{
  float maxPenetration = 0.f;
  int edgeWithMaxPenetration = -1;
  std::array<Vec3f, 12> points;
  std::array<Vec3f, 12> normals;

  auto edges = getEdges(getVertices(box1, box1Transform));
  for (size_t i = 0; i < 12; ++i) {
    //std::cout << "Edge " << i << ": (" << edges[i].A << "), (" << edges[i].B << ")\n";

    float penetration = edgeBoxPenetration(box2, worldToBox2Space, box2ToWorldSpace, edges[i],
      points[i], normals[i]);

    if (penetration > maxPenetration) {
      edgeWithMaxPenetration = i;
      maxPenetration = penetration;
    }
  }

  if (edgeWithMaxPenetration == -1) {
    return false;
  }

  contact.normal = normals[edgeWithMaxPenetration];
  contact.penetration = maxPenetration;
  contact.point = points[edgeWithMaxPenetration];
  contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
  contact.toContactSpace = contact.fromContactSpace.t();

  //std::cout << "Found edge with max penetration (index " << edgeWithMaxPenetration << ")\n";
  //std::cout << "Normal: " << contact.normal << "\n";
  //std::cout << "Point: " << contact.point << "\n";
  //std::cout << "Penetration: " << contact.penetration << "\n";

  return true;
}

std::vector<Contact> SysCollisionImpl::generateContacts(const std::vector<CollisionPair>& pairs)
{
  std::vector<Contact> allContacts;

  for (auto& pair : pairs) {
    // TODO: Not all entities have a collision box
    assert(pair.A.box != nullptr);
    assert(pair.B.box != nullptr);

    //std::cout << "Checking for contacts between " << pair.A.entityId << " and " << pair.B.entityId << "\n";

    auto boxAToWorldSpace = pair.A.localTransform->transform *
      pair.A.box->boundingBox.transform;
    auto worldToBoxASpace = inverse(boxAToWorldSpace);
    auto boxBToWorldSpace = pair.B.localTransform->transform *
      pair.B.box->boundingBox.transform;
    auto worldToBoxBSpace = inverse(boxBToWorldSpace);

    std::array<Contact, 3> contacts{};  // TODO: No need for array

    if (checkForPointContacts(pair.A.box->boundingBox, pair.A.localTransform->transform,
      pair.B.box->boundingBox, boxBToWorldSpace, worldToBoxBSpace, contacts[0])) {

      //std::cout << "Point contact A->B, IDs " << pair.A.entityId << "->" << pair.B.entityId << "\n";

      contacts[0].A = pair.A;
      contacts[0].B = pair.B;

      allContacts.push_back(contacts[0]);
    }
    else if (checkForPointContacts(pair.B.box->boundingBox,
      pair.B.localTransform->transform, pair.A.box->boundingBox, boxAToWorldSpace,
      worldToBoxASpace, contacts[1])) {

      //std::cout << "Point contact B->A, IDs " << pair.B.entityId << "->" << pair.A.entityId << "\n";

      contacts[1].A = pair.B;
      contacts[1].B = pair.A;

      allContacts.push_back(contacts[1]);
    }
    else if (checkForEdgeContacts(pair.A.box->boundingBox, pair.A.localTransform->transform,
      pair.B.box->boundingBox, boxBToWorldSpace, worldToBoxBSpace, contacts[2])) {

      //std::cout << "Edge contact A->B, IDs " << pair.A.entityId << "->" << pair.B.entityId << "\n";

      contacts[2].A = pair.A;
      contacts[2].B = pair.B;

      allContacts.push_back(contacts[2]);
    }
  }

  return allContacts;
}

void updateTransform(CLocalTransform& localT, CSpatialFlags& spatialFlags, const Vec3f& linearV,
  const Vec3f& angularV)
{
  auto translation = translationMatrix4x4(linearV);
  auto angle = angularV.magnitude();
  auto rotation = rotationMatrix4x4(angularV.normalise(), angle);

  auto currentOffset = getTranslation(localT.transform);

  auto t = translationMatrix4x4(currentOffset) * translation * rotation *
    translationMatrix4x4(-currentOffset) * localT.transform;

  localT.transform = t;
  spatialFlags.flags.set(SpatialFlags::Dirty);
}

// Constructs a matrix that calculates the velocity induced by a given impulse.
// Its inverse is a matrix that calculates the required impulse for a given velocity.
//
// I is the inverse inertia tensor in world space
// relativePoint is the contact point relative to the centre of mass
inline Mat3x3f computeVelocityMatrix(float inverseMass, const Mat3x3f& I,
  const Vec3f& relativePoint)
{
  Mat3x3f Q = skewSymmetricMatrix(relativePoint);
  Mat3x3f M = scaleMatrix<3>(inverseMass, false);
  return (Q * -1.f) * I * Q + M;
}

void applyPositionDelta(const ObjectComponents& obj, const Vec3f& point, const Vec3f& delta)
{
  Mat3x3f I = transformTensor(obj.dynamic->inverseInertialTensor, obj.localTransform->transform);

  auto origin = getTranslation(obj.localTransform->transform);
  auto pointRel = point - (origin + obj.dynamic->centreOfMass);
  Mat3x3f velocityMatrix = computeVelocityMatrix(obj.dynamic->inverseMass, I, pointRel);
  Mat3x3f impulseMatrix = inverse(velocityMatrix);
  Vec3f impulse = impulseMatrix * delta;
  Vec3f linearV = impulse * obj.dynamic->inverseMass;
  Vec3f angularV = I * pointRel.cross(impulse);

  //float angularVMagnitude = angularV.magnitude();
  //float linearVMagnitude = linearV.magnitude();
  //float totalVMagnitude = angularVMagnitude + linearVMagnitude;

  //std::cout << angularV.magnitude() << "\n";
  //float maxAngularV = 0.03f;
  //if (angularVMagnitude > maxAngularV) {
  //  std::cout << "Big angular V! " << angularVMagnitude << "\n";
  //  angularV = (angularV / angularVMagnitude) * maxAngularV;
  //  linearV = (linearV / linearVMagnitude) * (totalVMagnitude - maxAngularV);
  //}

  updateTransform(*obj.localTransform, *obj.spatialFlags, linearV, angularV);
}

void resolveInterpenetrationDynamicStatic(const Contact& contact)
{
  auto& obj = contact.A.dynamic == nullptr ? contact.B : contact.A;

  if (obj.dynamic->inverseMass != 0.f) {
    auto outDir = contact.normal;

    const float margin = -G * 0.f;
    auto delta = outDir * (contact.penetration + margin);

    applyPositionDelta(obj, contact.point, delta);
  }
}

void resolveInterpenetrationDynamicDynamic(const Contact& contact)
{
  auto& A = contact.A;
  auto& B = contact.B;

  float totalInvMass = A.dynamic->inverseMass + B.dynamic->inverseMass;
  DBG_ASSERT(totalInvMass != 0.f, "Cannot collide two objects of infinite mass");

  const float margin = -G * 0.f;

  if (A.dynamic->inverseMass != 0.f) {
    float a = (A.dynamic->inverseMass / totalInvMass) * (contact.penetration + margin);
    auto outDir = contact.normal;

    applyPositionDelta(A, contact.point, outDir * a);
  }

  if (B.dynamic->inverseMass != 0.f) {
    float b = (B.dynamic->inverseMass / totalInvMass) * (contact.penetration + margin);
    auto outDir = -contact.normal;

    applyPositionDelta(B, contact.point, outDir * b);
  }
}

void resolveInterpenetration(const Contact& contact)
{
  int numStatic = 0;
  if (contact.A.dynamic == nullptr) {
    ++numStatic;
  }
  if (contact.B.dynamic == nullptr) {
    ++numStatic;
  }

  DBG_ASSERT(numStatic != 2, "Cannot resolve penetration between 2 static objects");

  if (numStatic == 0) {
    resolveInterpenetrationDynamicDynamic(contact);
  }
  else {
    assert(numStatic == 1);
    resolveInterpenetrationDynamicStatic(contact);
  }
}

// TODO: Rename
void applyFriction(Vec3f& impulse, float friction)
{
  float planarImpulse = sqrt(impulse[0] * impulse[0] + impulse[2] * impulse[2]);
  if (planarImpulse > friction * impulse[1]) {
    impulse[0] /= planarImpulse;
    impulse[2] /= planarImpulse;
    impulse[0] *= friction * impulse[1];
    impulse[2] *= friction * impulse[1];
  }
}

void resolveVelocitiesDynamicStatic(const Contact& contact)
{
  auto& dynaObj = contact.A.dynamic == nullptr ? contact.B : contact.A;
  auto& statObj = contact.A.dynamic == nullptr ? contact.A : contact.B;

  //std::cout << "Waking up " << dynaObj.entityId << "\n";
  dynaObj.dynamic->idle = false;

  //dynaObj.dynamic->linearVelocity -= { 0.f, G, 0.f };
  dynaObj.dynamic->linearVelocity -= dynaObj.dynamic->linearAccelerationOfLastUpdate;
  dynaObj.dynamic->linearAccelerationOfLastUpdate = {};
  
  auto origin = getTranslation(dynaObj.localTransform->transform);
  auto pointRel = contact.point - (origin + dynaObj.dynamic->centreOfMass);
  auto totalWorldSpaceV = dynaObj.dynamic->linearVelocity +
    dynaObj.dynamic->angularVelocity.cross(pointRel);
  auto contactSpaceV = contact.toContactSpace * totalWorldSpaceV;

  float d = contact.A.dynamic == nullptr ? 1.f : -1.f;
  auto contactSpaceSeparatingV = contactSpaceV * d; // TODO: Rename to closing V?

  if (contactSpaceSeparatingV[1] < 0.f) {
    //contactSpaceSeparatingV[1] = 0.f;
    //std::cout << "Hello\n";
    //return;
  }

  float r = dynaObj.collision->restitution + statObj.collision->restitution;
  Vec3f desiredContactSpaceSeparatingV = { 0.f, contactSpaceSeparatingV[1] * -r, 0.f };
  //desiredContactSpaceSeparatingV += Vec3f{ 0.f, (contact.toContactSpace * dynaObj.dynamic->linearAccelerationOfLastUpdate)[1], 0.f } * d;
  //dynaObj.dynamic->linearAccelerationOfLastUpdate = {};
  auto desiredContactSpaceDv = contactSpaceSeparatingV - desiredContactSpaceSeparatingV;
  //desiredContactSpaceDv -= contact.toContactSpace * Vec3f{ 0.f, G, 0.f };

  Mat3x3f I = transformTensor(dynaObj.dynamic->inverseInertialTensor,
    dynaObj.localTransform->transform);

  Mat3x3f velocityMatrix = contact.fromContactSpace *
    computeVelocityMatrix(dynaObj.dynamic->inverseMass, I, pointRel) * contact.toContactSpace;
  Mat3x3f impulseMatrix = inverse(velocityMatrix);

  Vec3f contactSpaceImpulse = impulseMatrix * desiredContactSpaceDv;

  auto impulse = contact.fromContactSpace * contactSpaceImpulse;

  float friction = 0.5f * (dynaObj.collision->friction + statObj.collision->friction);
  //applyFriction(impulse, friction);

  float angularDamping = 1.f;

  //dynaObj.dynamic->linearVelocity = {};
  //dynaObj.dynamic->angularVelocity = {};
  dynaObj.dynamic->pendingLinearVelocity += (impulse * -1.f * d) * dynaObj.dynamic->inverseMass;
  dynaObj.dynamic->pendingAngularVelocity += I * pointRel.cross(impulse * -1.f * d) * angularDamping;
  dynaObj.dynamic->pendingN++;
}

void resolveVelocitiesDynamicDynamic(const Contact& contact)
{
  auto& A = contact.A;
  auto& B = contact.B;

  //std::cout << "Waking up " << A.entityId << "\n";
  //std::cout << "Waking up " << B.entityId << "\n";
  A.dynamic->idle = false;
  B.dynamic->idle = false;

  //A.dynamic->linearVelocity -= { 0.f, G, 0.f };
  //B.dynamic->linearVelocity -= { 0.f, G, 0.f };

  auto aOrigin = getTranslation(A.localTransform->transform);
  auto aPointRel = contact.point - (aOrigin + A.dynamic->centreOfMass);
  auto aTotalWorldSpaceV = A.dynamic->linearVelocity + A.dynamic->angularVelocity.cross(aPointRel);
  auto aContactSpaceV = contact.toContactSpace * aTotalWorldSpaceV;

  auto bOrigin = getTranslation(B.localTransform->transform);
  auto bPointRel = contact.point - (bOrigin + B.dynamic->centreOfMass);
  auto bTotalWorldSpaceV = B.dynamic->linearVelocity + B.dynamic->angularVelocity.cross(bPointRel);
  auto bContactSpaceV = contact.toContactSpace * bTotalWorldSpaceV;

  auto contactSpaceSeparatingV = bContactSpaceV - aContactSpaceV;

  float r = A.collision->restitution + B.collision->restitution;
  Vec3f desiredContactSpaceSeparatingV = { 0.f, contactSpaceSeparatingV[1] * -r, 0.f };
  //desiredContactSpaceSeparatingV -= { 0.f, (contact.toContactSpace * A.dynamic->linearAccelerationOfLastUpdate)[1], 0.f };
  //desiredContactSpaceSeparatingV += { 0.f, (contact.toContactSpace * B.dynamic->linearAccelerationOfLastUpdate)[1], 0.f };
  //A.dynamic->linearAccelerationOfLastUpdate = {};
  //B.dynamic->linearAccelerationOfLastUpdate = {};
  auto desiredContactSpaceDv = contactSpaceSeparatingV - desiredContactSpaceSeparatingV;

  Mat3x3f aI = transformTensor(A.dynamic->inverseInertialTensor, A.localTransform->transform);
  Mat3x3f bI = transformTensor(B.dynamic->inverseInertialTensor, B.localTransform->transform);

  Mat3x3f aVelocityMatrix = computeVelocityMatrix(A.dynamic->inverseMass, aI, aPointRel);
  Mat3x3f bVelocityMatrix = computeVelocityMatrix(B.dynamic->inverseMass, bI, bPointRel);

  Mat3x3f velocityMatrix = contact.fromContactSpace * (aVelocityMatrix + bVelocityMatrix)
    * contact.toContactSpace;
  Mat3x3f impulseMatrix = inverse(velocityMatrix);

  Vec3f contactSpaceImpulse = impulseMatrix * desiredContactSpaceDv;

  auto impulse = contact.fromContactSpace * contactSpaceImpulse;

  float friction = 0.5f * (A.collision->friction + B.collision->friction);
  applyFriction(impulse, friction);

  const float angularDamping = 1.f;

  //A.dynamic->linearVelocity = {};
  //A.dynamic->angularVelocity = {};
  A.dynamic->pendingLinearVelocity += impulse * A.dynamic->inverseMass;
  A.dynamic->pendingAngularVelocity += aI * aPointRel.cross(impulse) * angularDamping;
  A.dynamic->pendingN++;

  //B.dynamic->linearVelocity = {};
  //B.dynamic->angularVelocity = {};
  B.dynamic->pendingLinearVelocity += -impulse * B.dynamic->inverseMass;
  B.dynamic->pendingAngularVelocity += bI * bPointRel.cross(-impulse) * angularDamping;
  B.dynamic->pendingN++;
}

void resolveVelocities(const Contact& contact)
{
  int numStatic = 0;
  if (contact.A.dynamic == nullptr) {
    ++numStatic;
  }
  if (contact.B.dynamic == nullptr) {
    ++numStatic;
  }

  if (numStatic == 2) {
    return;
  }

  if (numStatic == 0) {
    resolveVelocitiesDynamicDynamic(contact);
  }
  else {
    assert(numStatic == 1);
    resolveVelocitiesDynamicStatic(contact);
  }
}

void SysCollisionImpl::integrate()
{
  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CLocalTransform, CCollision, CCollisionDynamic
  >();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localT = group.components<CLocalTransform>();
    auto dynamicComps = group.components<CCollisionDynamic>();
    auto entityIds = group.entityIds();

    for (size_t i = 0; i < flagsComps.size(); ++i) {
      auto& flags = flagsComps[i].flags;
      auto& dynamic = dynamicComps[i];

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)
        && !dynamic.idle && dynamic.inverseMass != 0.f) {

        if (dynamic.pendingN != 0) {
          dynamic.linearVelocity += dynamic.pendingLinearVelocity / static_cast<float>(dynamic.pendingN);
          dynamic.angularVelocity += dynamic.pendingAngularVelocity / static_cast<float>(dynamic.pendingN);
          dynamic.pendingLinearVelocity = {};
          dynamic.pendingAngularVelocity = {};
          dynamic.pendingN = 0;
        }

        //std::cout << "id: " << entityIds[i] << "\n";
        //std::cout << "v: " << dynamic.linearVelocity << "\n";
        //std::cout << "a: " << dynamic.angularVelocity << "\n";
        //std::cout << "v sq: " << dynamic.linearVelocity.squareMagnitude() << "\n";
        //std::cout << "a sq: " << dynamic.angularVelocity.squareMagnitude() << "\n";

        if (dynamic.linearVelocity.squareMagnitude() < 0.01f &&
          dynamic.angularVelocity.squareMagnitude() < 0.00001f) {

          if (++dynamic.framesIdle > TICKS_PER_SECOND) {
            DBG_LOG(m_logger, "Setting idle");
            dynamic.idle = true;
            dynamic.framesIdle = 0;
            dynamic.linearVelocity = {};
            dynamic.angularVelocity = {};
            dynamic.linearAcceleration = {};
            dynamic.angularAcceleration = {};
            continue;
          }
        }

        Vec3f totalForce;
        Vec3f totalTorque;

        for (size_t f = 0; f < MAX_FORCES; ++f) {
          auto& force = dynamic.linearForces[f];
          auto& torque = dynamic.torques[f];

          if (force.lifetime > 0) {
            totalForce += force.force;
            --force.lifetime;
          }

          if (torque.lifetime > 0) {
            totalTorque += torque.force;
            --torque.lifetime;
          }
        }

        updateTransform(localT[i], flagsComps[i], dynamic.linearVelocity, dynamic.angularVelocity);

        dynamic.linearAccelerationOfLastUpdate = dynamic.linearAcceleration;

        dynamic.linearVelocity += dynamic.linearAcceleration;
        dynamic.linearAcceleration = totalForce * dynamic.inverseMass;

        dynamic.angularVelocity += dynamic.angularAcceleration;
        dynamic.angularAcceleration = dynamic.inverseInertialTensor * totalTorque;
      }
    }
  }
}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{
  size_t maxIterations = 1;

  auto pairs = findPossibleCollisions();

  size_t i = 0;
  for (; i < maxIterations; ++i) {
    //auto pairs = findPossibleCollisions();
    auto contacts = generateContacts(pairs);
    if (contacts.empty()) {
      break;
    }
    for (auto& contact : contacts) {
      resolveInterpenetration(contact);
      resolveVelocities(contact);
    }
  }

  if (i == maxIterations) {
    //DBG_LOG(m_logger, "Max iterations reached");
  }
  integrate();
}

void SysCollisionImpl::processEvent(const Event& event)
{
  // TODO
}

} // namespace

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysCollisionImpl>(ecs, eventSystem, logger);
}

} // namespace lithic3d
