#include "product_activation.hpp"
#include "drm.hpp"
#include "logger.hpp"
#include "sys_spatial.hpp"
#include "systems.hpp"
#include "sys_render.hpp"

namespace
{

class ProductActivationImpl : public ProductActivation
{
  public:
    ProductActivationImpl(Ecs& ecs, EventSystem& eventSystem, Drm& drm, Logger& logger);

    EntityId root() const override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    Drm& m_drm;
    EntityId m_rootId;

    EntityId constructRoot();
    EntityId constructPrompt();
    EntityId constructTextbox();
};

ProductActivationImpl::ProductActivationImpl(Ecs& ecs, EventSystem& eventSystem, Drm& drm,
  Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_drm(drm)
{
  m_rootId = constructRoot();
  constructPrompt();
  constructTextbox();
}

EntityId ProductActivationImpl::root() const
{
  return m_rootId;
}

EntityId ProductActivationImpl::constructRoot()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float, 4>(),
    .parent = sysSpatial.root(),
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  return id;
}

EntityId ProductActivationImpl::constructPrompt()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  Vec2f pos{ 0.2f, 0.7f };
  Vec2f size{ 0.022f, 0.044f };

  SpatialData spatial{
    .transform = translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
      scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f }),
    .parent = m_rootId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  TextData render{
    .viewport = MAIN_VIEWPORT,
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .text = "Enter 8-digit activation key",
    .zIndex = 1,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  sysRender.addEntity(id, render);

  return id;
}

EntityId ProductActivationImpl::constructTextbox()
{
  // TODO
  return NULL_ENTITY;
}

} // namespace

ProductActivationPtr createProductActivation(Ecs& ecs, EventSystem& eventSystem, Drm& drm,
  Logger& logger)
{
  return std::make_unique<ProductActivationImpl>(ecs, eventSystem, drm, logger);
}
