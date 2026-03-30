#include "player.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;

namespace
{

class PlayerImpl : public Player
{
  public:
    PlayerImpl(Ecs& ecs, RenderResourceLoader& renderResourceLoader, ModelLoader& modelLoader);

    void update() override;
    const Mat4x4f& getTransform() const override;
    Vec3f getPosition() const override;
    void setPosition(const Vec3f& position) override;
    Vec3f getDirection() const override;
    virtual void translate(const Vec3f& delta) override;
    virtual void rotate(float deltaPitch, float deltaYaw) override;
    EntityId getId() const override;

  private:
    Ecs& m_ecs;
    RenderResourceLoader& m_renderResourceLoader;
    ModelLoader& m_modelLoader;
    Camera3d& m_camera;
    EntityId m_entityId;
    float m_tallness = metresToWorldUnits(1.7f);

    void syncCamera();
};

PlayerImpl::PlayerImpl(Ecs& ecs, RenderResourceLoader& renderResourceLoader, ModelLoader& modelLoader)
  : m_ecs(ecs)
  , m_renderResourceLoader(renderResourceLoader)
  , m_modelLoader(modelLoader)
  , m_camera(m_ecs.system<SysRender3d>().camera())
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysCollision = m_ecs.system<SysCollision>();

  m_entityId = m_ecs.idGen().getNewEntityId();
  m_ecs.componentStore().allocate<DSpatial, DModel, DCapsule>(m_entityId);

  Vec3f size{ 1.f, 1.f, 1.f };

  DSpatial spatial{
    .transform = identityMatrix<4>(),
    .parent = sysSpatial.root(),
    .enabled = true,
    .aabb = {
      .min = -size * 0.5f,
      .max = size * 0.5f
    }
  };

  sysSpatial.addEntity(m_entityId, spatial);

  DCapsule collision{
    .inverseMass = 0.1f,
    .restitution = 0.2f,
    .friction = 0.4f,
    .centreOfMass = { 0.f, 0.f, 0.f },
    .capsule{
      .radius = metresToWorldUnits(0.5f),
      .height = m_tallness,
      .translation{}
    }
  };
  sysCollision.addEntity(m_entityId, collision);
}

EntityId PlayerImpl::getId() const
{
  return m_entityId;
}

const Mat4x4f& PlayerImpl::getTransform() const
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  return sysSpatial.getLocalTransform(m_entityId);
}

Vec3f PlayerImpl::getPosition() const
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  return getTranslation(sysSpatial.getLocalTransform(m_entityId));
}

Vec3f PlayerImpl::getDirection() const
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto rotation = getRotation3x3(sysSpatial.getLocalTransform(m_entityId));
  return rotation * Vec3f{ 0.f, 0.f, -1.f };
}

void PlayerImpl::translate(const Vec3f& delta)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  sysSpatial.translateEntityLocal(m_entityId, delta);
}

void PlayerImpl::rotate(float deltaPitch, float deltaYaw)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();

  auto direction = getDirection();
  auto pitch = rotationMatrix3x3(direction.cross(Vec3f{0, 1, 0}), deltaPitch);
  auto yaw = rotationMatrix3x3(Vec3f{0, 1, 0}, -deltaYaw);
  sysSpatial.rotateEntityLocal(m_entityId, yaw * pitch);
}

void PlayerImpl::setPosition(const Vec3f& position)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  sysSpatial.setLocalTransform(m_entityId, translationMatrix4x4(position));
}

void PlayerImpl::update()
{
  syncCamera();
}

void PlayerImpl::syncCamera()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto t = sysSpatial.getLocalTransform(m_entityId);
  m_camera.setTransform(translationMatrix4x4({ 0.f, m_tallness, 0.f }) * t);
}

} // namespace

PlayerPtr createPlayer(Ecs& ecs, RenderResourceLoader& renderResourceLoader,
  ModelLoader& modelLoader)
{
  return std::make_unique<PlayerImpl>(ecs, renderResourceLoader, modelLoader);
}
