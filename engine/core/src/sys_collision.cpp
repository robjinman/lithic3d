#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"
#include <iostream> // TODO

namespace lithic3d
{
namespace
{

uint32_t findFreeIndex(const std::array<Force, MAX_FORCES>& array)
{
  for (size_t i = 0; i < MAX_FORCES; ++i) {
    if (array[i].lifetime == 0) {
      return i;
    }
  }
  return MAX_FORCES;
}

struct ObjectComponents
{
  CCollision* collision = nullptr;
  CGlobalTransform* globalTransform = nullptr;
  CLocalTransform* localTransform = nullptr;
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
};

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DCollision& data) override;
    void addEntity(EntityId id, const DTerrainChunk data) override;

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

    void applyForce(CCollision& comp, const Force& force);
    void applyTorque(CCollision& comp, const Force& torque);
    void applyGravity(CCollision& comp);
    std::vector<CollisionPair> findPossibleCollisions();
    std::vector<Contact> generateContacts(const std::vector<CollisionPair>& pairs);
    void resolveContacts(const std::vector<Contact>& contacts);
    void integrate();
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

void SysCollisionImpl::addEntity(EntityId id, const DCollision& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  CCollision comp{
    .boundingBox = data.boundingBox,
    .inverseMass = data.inverseMass,
    .centreOfMass = data.centreOfMass,
    .restitution = data.restitution,
    .linearForces{},
    .linearAcceleration{},
    .linearVelocity{},
    .torques{},
    .angularAcceleration{},
    .angularVelocity{}
  };

  if (comp.inverseMass != 0) {
    applyGravity(comp);
  }

  componentStore.instantiate<CCollision>(id) = comp;
}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CGlobalTransform>(componentStore, id);
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

void SysCollisionImpl::applyGravity(CCollision& comp)
{
  if (comp.inverseMass == 0.f) {
    return;
  }

  float a = -metresToWorldUnits(9.8f) / (TICKS_PER_SECOND * TICKS_PER_SECOND);
  float f = (1.f / comp.inverseMass) * a;

  applyForce(comp, {
    .force{ 0.f, f, 0.f },
    .lifetime = std::numeric_limits<uint32_t>::max()
  });
}

void SysCollisionImpl::applyForce(EntityId id, const Vec3f& force, float seconds)
{
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
  applyForce(comp, {
    .force = force,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyForce(CCollision& comp, const Force& force)
{
  uint32_t i = findFreeIndex(comp.linearForces);
  if (i == MAX_FORCES) {
    m_logger.warn("Max forces exceeded on object");
    return;
  }
  comp.linearForces[i] = force;
}

void SysCollisionImpl::applyTorque(EntityId id, const Vec3f& torque, float seconds)
{
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
  applyTorque(comp, {
    .force = torque,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyTorque(CCollision& comp, const Force& torque)
{
  uint32_t i = findFreeIndex(comp.torques);
  if (i == MAX_FORCES) {
    m_logger.warn("Max torques exceeded on object");
    return;
  }
  comp.torques[i] = torque;
}

void SysCollisionImpl::setStationary(EntityId id)
{
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
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
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
  if (comp.inverseMass == 0.f && inverseMass != 0.f) {
    comp.inverseMass = inverseMass;
    applyGravity(comp);
  }
  else {
    comp.inverseMass = inverseMass;
  }
}

std::vector<CollisionPair> SysCollisionImpl::findPossibleCollisions()
{
  // Extremely naive collision algorithm
  // TODO: Use something like sweep & prune instead: https://leanrada.com/notes/sweep-and-prune/

  std::vector<CollisionPair> pairs;

  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CBoundingBox, CCollision
  >();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localTs = group.components<CLocalTransform>();
    auto globalTs = group.components<CGlobalTransform>();
    auto boundingBoxes = group.components<CBoundingBox>();
    auto collisionComps = group.components<CCollision>();
    auto entityIds = group.entityIds();

    for (size_t i = 0; i < entityIds.size(); ++i) {
      auto& flags = flagsComps[i].flags;

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {
        for (size_t j = i + 1; j < entityIds.size(); ++j) {
          auto& flagsJ = flagsComps[j].flags;

          if (flagsJ.test(SpatialFlags::ParentEnabled) && flagsJ.test(SpatialFlags::Enabled)) {
            auto& box1 = boundingBoxes[i].worldSpaceAabb;
            auto& box2 = boundingBoxes[j].worldSpaceAabb;

            if (box1.max[0] >= box2.min[0] && box1.min[0] <= box2.max[0] &&
              box1.max[1] >= box2.min[1] && box1.min[1] <= box2.max[1] &&
              box1.max[2] >= box2.min[2] && box1.min[2] <= box2.max[2]) {

              pairs.push_back({
                .A = {
                  .collision = &collisionComps[i],
                  .globalTransform = &globalTs[i],
                  .localTransform = &localTs[i]
                },
                .B = {
                  .collision = &collisionComps[j],
                  .globalTransform = &globalTs[j],
                  .localTransform = &localTs[j]
                },
              });
            }
          }
        }
      }
    }
  }

  return pairs;
}

inline std::array<Vec4f, 8> vertices(const BoundingBox& box, const Mat4x4f& transform)
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
    transform * Vec4f{ box.min[0], box.min[1], box.min[2], 1.f },  // A  
    transform * Vec4f{ box.max[0], box.min[1], box.min[2], 1.f },  // B
    transform * Vec4f{ box.min[0], box.max[1], box.min[2], 1.f },  // C
    transform * Vec4f{ box.max[0], box.max[1], box.min[2], 1.f },  // D
    transform * Vec4f{ box.min[0], box.min[1], box.max[2], 1.f },  // E
    transform * Vec4f{ box.max[0], box.min[1], box.max[2], 1.f },  // F
    transform * Vec4f{ box.min[0], box.max[1], box.max[2], 1.f },  // G
    transform * Vec4f{ box.max[0], box.max[1], box.max[2], 1.f }   // H
  };
}

// Returns penetration depth or 0 if there's no penetration
float pointBoxPenetration(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const Mat4x4f& boxToWorldSpace, const Vec3f& P, Vec3f& normal)
{
  auto Q = worldToBoxSpace * Vec4f{ P, { 1.f }};

  bool isInside = Q[0] > box.min[0] && Q[0] < box.max[0] &&
    Q[1] > box.min[1] && Q[1] < box.max[1] &&
    Q[2] > box.min[2] && Q[2] < box.max[2];

  if (!isInside) {
    return 0.f;
  }

  float minPenetration = std::numeric_limits<float>::max();
  int nearestFace = 0; // 0 min-x, 1 max-x, 2 min-y, 3 max-y, 4 min-z, 5 max-z

  // min-x (left) face
  float penetration = Q[0] - box.min[0];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 0;
  }
  // max-x (right) face
  penetration = box.max[0] - Q[0];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 1;
  }
  // min-y (bottom) face
  penetration = Q[1] - box.min[1];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 2;
  }
  // max-y (top) face
  penetration = box.max[1] - Q[1];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 3;
  }
  // min-z (far) face
  penetration = Q[2] - box.min[2];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 4;
  }
  // max-z (near) face
  penetration = box.max[2] - Q[2];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 5;
  }

  // Set w components to zero to ignore translation
  switch (nearestFace) {
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
      assert(false);
  }

  return minPenetration;
}

// Check if any of box1's vertices intersect box2 and generate a contact
bool checkPointPlaneContact(const BoundingBox& box1, const Mat4x4f& box1Transform,
  const BoundingBox& box2, const Mat4x4f& box2ToWorldSpace, const Mat4x4f& worldToBox2Space,
  Contact& contact)
{
  float maxPenetration = 0.f;
  int vertWithMaxPenetration = -1;
  std::array<Vec3f, 8> normals;

  auto verts = vertices(box1, box1Transform);
  for (size_t i = 0; i < 8; ++i) {
    Vec3f worldSpaceVert = verts[i].sub<3>();
    float penetration = pointBoxPenetration(box2, worldToBox2Space, box2ToWorldSpace,
      worldSpaceVert, normals[i]);
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
  contact.point = verts[vertWithMaxPenetration].sub<3>();

  return true;
}

bool checkEdgeEdgeContact(const BoundingBox& box1, const Mat4x4f& box1Transform,
  const BoundingBox& box2, const Mat4x4f& box2Transform, Contact& contact)
{
  // TODO
  return false;
}

std::vector<Contact> SysCollisionImpl::generateContacts(const std::vector<CollisionPair>& pairs)
{
  std::vector<Contact> contacts;

  for (auto& pair : pairs) {
    Contact contact{
      .A = pair.A,
      .B = pair.B,
      .point{},
      .normal{},
      .penetration = 0.f
    };

    auto boxAToWorldSpace = pair.A.globalTransform->transform *
      pair.A.collision->boundingBox.transform;
    auto worldToBoxASpace = inverse(boxAToWorldSpace);
    auto boxBToWorldSpace = pair.B.globalTransform->transform *
      pair.B.collision->boundingBox.transform;
    auto worldToBoxBSpace = inverse(boxBToWorldSpace);

    // TODO: checkEdgeEdgeContact

    if (checkPointPlaneContact(pair.A.collision->boundingBox, pair.A.globalTransform->transform,
      pair.B.collision->boundingBox, boxBToWorldSpace, worldToBoxBSpace, contact)) {

      contacts.push_back(contact);
    }
    else if (checkPointPlaneContact(pair.B.collision->boundingBox,
      pair.B.globalTransform->transform, pair.A.collision->boundingBox, boxAToWorldSpace,
      worldToBoxASpace, contact)) {

      contacts.push_back(contact);
    }
  }

  return contacts;
}

void resolveInterpenetration(const Contact& contact)
{
  float totalInvMass = contact.A.collision->inverseMass + contact.B.collision->inverseMass;
  float a = contact.A.collision->inverseMass / totalInvMass;
  float b = contact.B.collision->inverseMass / totalInvMass;
  auto da = contact.normal * a * contact.penetration; // TODO
  auto db = -contact.normal * b * contact.penetration;

  // TODO: Consider moving objects backward along velocity vectors?

  auto& transA = *contact.A.localTransform;
  auto& transB = *contact.B.localTransform;
  transA.transform = translationMatrix4x4(da) * transA.transform;
  transB.transform = translationMatrix4x4(db) * transB.transform;

  // TODO: Set dirty flag
}

void resolveVelocities(const Contact& contact)
{
  // TODO:
  // * Calculate separating velocity between contacts
  // * Calculate the desired change in linear velocity
  // * For each object, calculate the change in linear and angular velocities that applying 1 unit
  //   of impulse would produce.
  // * Calculate the impulse required to achieve the desired separation velocity
  // * Apply the impulse to each object

  auto& A = contact.A;
  auto& B = contact.B;

  auto aOrigin = getTranslation(A.globalTransform->transform);
  auto aPointRel = contact.point - aOrigin;
  auto aTotalWorldSpaceV = A.collision->linearVelocity +
    A.collision->angularVelocity.cross(aPointRel);
  auto aWorldSpaceV = -contact.normal * aTotalWorldSpaceV.dot(contact.normal);

  auto bOrigin = getTranslation(B.globalTransform->transform);
  auto bPointRel = contact.point - bOrigin;
  auto bTotalWorldSpaceV = B.collision->linearVelocity +
    B.collision->angularVelocity.cross(bPointRel);
  auto bWorldSpaceV = contact.normal * bTotalWorldSpaceV.dot(contact.normal);

  auto totalClosingV = aWorldSpaceV + bWorldSpaceV;

  float restitution = 0.5f; // TODO
  auto desiredClosingV = totalClosingV * -restitution;
  auto desiredDeltaV = totalClosingV - desiredClosingV;

  Mat3x3f aI = identityMatrix<3>();  // TODO: inverse inertia tensor
  Mat3x3f bI = identityMatrix<3>();  // TODO: inverse inertia tensor

  Vec3f aDvPerUnitImpulse = aI * aPointRel.cross(contact.normal) +
    contact.normal * A.collision->inverseMass;

  Vec3f bDvPerUnitImpulse = bI * bPointRel.cross(contact.normal) +
    contact.normal * B.collision->inverseMass;

  auto dvPerUnitImpulse = aDvPerUnitImpulse + bDvPerUnitImpulse;

  // Impulse needed to achieve desired delta v
  auto impulse = desiredDeltaV / dvPerUnitImpulse;

  // Apply the impulse

  A.collision->linearVelocity += impulse * A.collision->inverseMass;
  //A.collision->angularVelocity += aPointRel.cross(impulse);

  B.collision->linearVelocity += impulse * B.collision->inverseMass;
  //B.collision->angularVelocity += bPointRel.cross(impulse);
}

void SysCollisionImpl::resolveContacts(const std::vector<Contact>& contacts)
{
  for (auto& contact : contacts) {
    resolveInterpenetration(contact);
    resolveVelocities(contact);
  }
}

void SysCollisionImpl::integrate()
{
  auto groups = m_ecs.componentStore().components<CSpatialFlags, CLocalTransform, CCollision>();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localT = group.components<CLocalTransform>();
    auto collisionComps = group.components<CCollision>();

    for (size_t i = 0; i < flagsComps.size(); ++i) {
      auto& flags = flagsComps[i].flags;
      auto& collision = collisionComps[i];

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {
        Vec3f totalForce;
        Vec3f totalTorque;

        for (size_t f = 0; f < MAX_FORCES; ++f) {
          auto& force = collision.linearForces[f];
          auto& torque = collision.torques[f];

          if (force.lifetime > 0) {
            totalForce += force.force;
            --force.lifetime;
          }

          if (torque.lifetime > 0) {
            totalTorque += torque.force;
            --torque.lifetime;
          }
        }

        Mat3x3f I = identityMatrix<3>();  // TODO: inverse inertia tensor
        collision.angularAcceleration = I * totalTorque;
        collision.angularVelocity += collision.angularAcceleration;

        collision.linearAcceleration = totalForce * collision.inverseMass;
        collision.linearVelocity += collision.linearAcceleration;

        localT[i].transform = translationMatrix4x4(collision.linearVelocity) * localT[i].transform;
        flags.set(SpatialFlags::Dirty);
      }
    }
  }
}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{
  resolveContacts(generateContacts(findPossibleCollisions()));
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
