#include "render_system.hpp"
#include "renderer.hpp"
#include "spatial_system.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "exception.hpp"
#include "utils.hpp"
#include "time.hpp"
#include "file_system.hpp"
#include <cassert>
#include <functional>

using render::Renderer;
using render::Mesh;
using render::MeshPtr;
using render::Material;
using render::MaterialPtr;
using render::MeshFeatureSet;
using render::MaterialFeatureSet;
using render::MeshHandle;
namespace MaterialFeatures = render::MaterialFeatures;
namespace MeshFeatures = render::MeshFeatures;
using render::MaterialHandle;
using render::TexturePtr;
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

struct CRenderData
{
  // TODO: If component is frequently accessed, consider copying data into this struct
  CRenderPtr component;

  // TODO: Allow entities to share meshes/materials?
  MeshHandle mesh;
  MaterialHandle material;
};

class RenderSystemImpl : public RenderSystem
{
  public:
    RenderSystemImpl(const SpatialSystem&, Renderer& renderer, const FileSystem& fileSystem,
      Logger& logger);

    void start() override;
    double frameRate() const override;

    Camera& camera() override;
    const Camera& camera() const override;

    void addComponent(ComponentPtr component) override;
    void removeComponent(EntityId entityId) override;
    bool hasComponent(EntityId entityId) const override;
    CRender& getComponent(EntityId entityId) override;
    const CRender& getComponent(EntityId entityId) const override;
    void update() override;

  private:
    Logger& m_logger;
    Camera m_camera;
    const SpatialSystem& m_spatialSystem;
    Renderer& m_renderer;
    const FileSystem& m_fileSystem;
    std::vector<CRenderData> m_data;
    std::vector<uint32_t> m_lookup;   // EntityId -> m_data index
    MaterialHandle m_textureAtlas;

    MeshHandle createMesh(const CRender& c) const;
};

RenderSystemImpl::RenderSystemImpl(const SpatialSystem& spatialSystem, Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_spatialSystem(spatialSystem)
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

  m_camera.setPosition(Vec3f{ 0.f, 0.f, 1.f });
}

void RenderSystemImpl::start()
{
  m_renderer.start();
}

double RenderSystemImpl::frameRate() const
{
  return m_renderer.frameRate();
}

MeshHandle RenderSystemImpl::createMesh(const CRender& c) const
{
  return m_renderer.addMesh(quad(c.size, c.textureRect));
}

void RenderSystemImpl::addComponent(ComponentPtr component)
{
  auto renderComp = CRenderPtr(dynamic_cast<CRender*>(component.release()));
  auto id = renderComp->id();

  auto mesh = createMesh(*renderComp);

  m_data.push_back(CRenderData{
    .component = std::move(renderComp),
    .mesh = mesh,
    .material = m_textureAtlas
  });

  if (id + 1 > m_lookup.size()) {
    m_lookup.resize(id + 1, 0);
  }
  m_lookup[id] = m_data.size() - 1;
}

void RenderSystemImpl::removeComponent(EntityId entityId)
{
  if (m_data.empty()) {
    return;
  }

  auto idx = m_lookup[entityId];
  auto last = m_data.size() - 1;

  auto lastId = m_data[last].component->id();

  std::swap(m_data[idx], m_data[last]);
  m_data.pop_back();

  m_lookup[entityId] = 0;
  m_lookup[lastId] = idx;
}

bool RenderSystemImpl::hasComponent(EntityId entityId) const
{
  return m_lookup.size() + 1 > entityId && m_lookup[entityId] != 0;
}

CRender& RenderSystemImpl::getComponent(EntityId entityId)
{
  return *m_data[m_lookup[entityId]].component;
}

const CRender& RenderSystemImpl::getComponent(EntityId entityId) const
{
  return *m_data[m_lookup[entityId]].component;
}

Camera& RenderSystemImpl::camera()
{
  return m_camera;
}

const Camera& RenderSystemImpl::camera() const
{
  return m_camera;
}

void RenderSystemImpl::update()
{
  try {
    m_renderer.beginFrame();
    m_renderer.beginPass(render::RenderPass::Overlay, m_camera.getPosition(), m_camera.getMatrix());

    for (auto& item : m_data) {
      auto& transform = m_spatialSystem.getComponent(item.component->id()).transform;
      auto screenSpaceTransform = screenToWorld(transform, m_renderer.getViewParams().aspectRatio);
      m_renderer.drawModel(item.mesh, item.material, screenSpaceTransform);
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

RenderSystemPtr createRenderSystem(const SpatialSystem& spatialSystem, Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<RenderSystemImpl>(spatialSystem, renderer, fileSystem, logger);
}
