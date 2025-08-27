#include "sys_render.hpp"
#include "renderer.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "exception.hpp"
#include "utils.hpp"
#include "time.hpp"
#include "file_system.hpp"
#include "sys_spatial.hpp"
#include <cassert>
#include <functional>
#include <map>

using render::Renderer;
using render::Mesh;
using render::MeshPtr;
using render::Material;
using render::MeshFeatureSet;
using render::MaterialFeatureSet;
using render::MeshHandle;
namespace MaterialFeatures = render::MaterialFeatures;
namespace MeshFeatures = render::MeshFeatures;
using render::MaterialHandle;
using render::RenderPass;
using render::Buffer;
using render::BufferUsage;
using render::toBytes;

namespace
{

Mat4x4f screenToWorld(const Mat4x4f& transform, float_t aspect)
{
  static Mat4x4f M = translationMatrix4x4(Vec3f{ -0.5f * aspect, -0.5f, 0.f });
  return M * transform;
}

MeshPtr quad(const Vec2f& size, const Rectf& uvRect)
{
  float_t w = size[0];
  float_t h = size[1];

  float_t u0 = uvRect.x;
  float_t v0 = uvRect.y;
  float_t u1 = u0 + uvRect.w;
  float_t v1 = v0 + uvRect.h;

  MeshPtr mesh = std::make_unique<Mesh>(MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  });
  mesh->featureSet.flags.set(MeshFeatures::Is2d);
  mesh->attributeBuffers = {
    Buffer{
      .usage = BufferUsage::AttrPosition,
      .data = toBytes(std::vector<Vec3f>{
        { 0, 0, 0 },
        { w, 0, 0 },
        { w, h, 0 },
        { 0, h, 0 }
      })
    },
    Buffer{
      .usage = BufferUsage::AttrNormal,
      .data = toBytes(std::vector<Vec3f>{
        { 0, 0, 1 },
        { 0, 0, 1 },
        { 0, 0, 1 },
        { 0, 0, 1 }
      })
    },
    Buffer{
      .usage = BufferUsage::AttrTexCoord,
      .data = toBytes(std::vector<Vec2f>{
        { u0, v1 },
        { u1, v1 },
        { u1, v0 },
        { u0, v0 }
      })
    }
  };
  mesh->indexBuffer = Buffer{
    .usage = BufferUsage::Index,
    .data = toBytes(std::vector<uint16_t>{
      0, 1, 2, 0, 2, 3
    })
  };

  return mesh;
}

using CRenderData = CRenderView;

class SysRenderImpl : public SysRender
{
  public:
    SysRenderImpl(ComponentStore& componentStore, Renderer& renderer, const FileSystem& fileSystem,
      Logger& logger);

    void start() override;
    double frameRate() const override;

    Camera& camera() override;
    const Camera& camera() const override;

    void addEntity(EntityId entityId, const CRender& component) override;
    void setZIndex(EntityId entityId, uint32_t zIndex) override;
    void setTextureRect(EntityId entityId, const Rectf& textureRect) override;
    void setVisible(EntityId entityId, bool visible) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override;
    void processEvent(const Event& event) override {}

  private:
    Logger& m_logger;
    ComponentStore& m_componentStore;
    Camera m_camera;
    Renderer& m_renderer;
    const FileSystem& m_fileSystem;
    MaterialHandle m_textureAtlas;
    MeshHandle m_mesh;
};

SysRenderImpl::SysRenderImpl(ComponentStore& componentStore, Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_componentStore(componentStore)
  , m_renderer(renderer)
  , m_fileSystem(fileSystem)
{
  MeshFeatureSet meshFeatures{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };
  meshFeatures.flags.set(MeshFeatures::Is2d);

  MaterialFeatureSet materialFeatures{};
  materialFeatures.flags.set(MaterialFeatures::HasTexture);

  m_renderer.compileShader(meshFeatures, materialFeatures);

  auto texture = render::loadTexture(m_fileSystem.readFile("textures/atlas.png"));

  auto material = std::make_unique<Material>(materialFeatures);
  material->texture.id = m_renderer.addTexture(std::move(texture));

  m_textureAtlas = m_renderer.addMaterial(std::move(material));

  m_mesh = m_renderer.addMesh(quad(Vec2f{ 1.f, 1.f }, Rectf{ 0.f, 0.f, 1.f, 1.f }));

  m_camera.setPosition(Vec3f{ 0.f, 0.f, 1.f });
}

void SysRenderImpl::start()
{
  m_renderer.start();
}

double SysRenderImpl::frameRate() const
{
  return m_renderer.frameRate();
}

void SysRenderImpl::addEntity(EntityId entityId, const CRender& data)
{
  m_componentStore.component<CRenderData>(entityId) = CRenderData{
    .textureRect = data.textureRect,
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true
  };
}

void SysRenderImpl::setZIndex(EntityId entityId, uint32_t zIndex)
{
  auto& renderComp = m_componentStore.component<CRenderView>(entityId);
  renderComp.zIndex = zIndex;
}

void SysRenderImpl::setTextureRect(EntityId entityId, const Rectf& textureRect)
{
  auto& renderComp = m_componentStore.component<CRenderView>(entityId);
  renderComp.textureRect = textureRect;
}

void SysRenderImpl::setVisible(EntityId entityId, bool visible)
{
  auto& renderComp = m_componentStore.component<CRenderView>(entityId);
  renderComp.visible = visible;
}

void SysRenderImpl::removeEntity(EntityId entityId)
{
}

bool SysRenderImpl::hasEntity(EntityId entityId) const
{
  return m_componentStore.hasComponentForEntity<CRenderData>(entityId);
}

Camera& SysRenderImpl::camera()
{
  return m_camera;
}

const Camera& SysRenderImpl::camera() const
{
  return m_camera;
}

void SysRenderImpl::update(Tick tick)
{
  try {
    m_renderer.beginFrame({ 0.f, 0.f, 0.f, 1.f });
    m_renderer.beginPass(render::RenderPass::Overlay, m_camera.getPosition(), m_camera.getMatrix());

    for (auto& group : m_componentStore.components<CRenderData>()) {
      size_t n = group.numEntities();
      auto renderComps = group.components<CRenderData>();
      auto globalTs = group.components<CGlobalTransform>();
      auto flags = group.components<CSpatialFlags>();
      auto& entityIds = group.entityIds();

      for (size_t i = 0; i < n; ++i) {
        auto& item = renderComps[i];
        if (!item.visible) {
          continue;
        }
        if (!flags[i].active) {
          continue;
        }

        auto& t = globalTs[i].transform;
        auto screenSpaceTransform = screenToWorld(t, m_renderer.getViewParams().aspectRatio);

        std::array<Vec2f, 4> uvCoords{
          Vec2f{ item.textureRect.x, item.textureRect.y + item.textureRect.h },
          Vec2f{ item.textureRect.x + item.textureRect.w, item.textureRect.y + item.textureRect.h },
          Vec2f{ item.textureRect.x + item.textureRect.w, item.textureRect.y },
          Vec2f{ item.textureRect.x, item.textureRect.y }
        };

        m_renderer.setOrderKey(item.zIndex);
        m_renderer.drawSprite(m_mesh, m_textureAtlas, uvCoords, item.colour,
          screenSpaceTransform);
      }
    }

    m_renderer.endPass();
    m_renderer.endFrame();
    m_renderer.checkError();
  }
  catch (const std::exception& e) {
    EXCEPTION(STR("Error rendering scene; " << e.what()));
  }
}

} // namespace

SysRenderPtr createSysRender(ComponentStore& componentStore, Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<SysRenderImpl>(componentStore, renderer, fileSystem, logger);
}
