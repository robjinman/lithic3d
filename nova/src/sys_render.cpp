#include "sys_render.hpp"
#include "renderer.hpp"
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

struct CRenderData
{
  Vec2f pos;
  uint32_t zIndex;
  MeshHandle mesh; // TODO: Share meshes or use instancing?
};

class SysRenderImpl : public SysRender
{
  public:
    SysRenderImpl(Renderer& renderer, const FileSystem& fileSystem, Logger& logger);

    void start() override;
    double frameRate() const override;

    Camera& camera() override;
    const Camera& camera() const override;

    void addEntity(EntityId entityId, const CRender& component) override;
    void moveEntity(EntityId entityId, const Vec2f& pos) override;
    void playAnimation(EntityId entityId, HashedString name) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update() override;
    void processEvent(const GameEvent& event) override {}

  private:
    Logger& m_logger;
    Camera m_camera;
    Renderer& m_renderer;
    const FileSystem& m_fileSystem;
    std::vector<CRenderData> m_data;
    std::vector<EntityId> m_ids;      // m_data index -> EntityId
    std::vector<uint32_t> m_lookup;   // EntityId -> m_data index
    MaterialHandle m_textureAtlas;

    MeshHandle createMesh(const CRender& c) const;
};

SysRenderImpl::SysRenderImpl(Renderer& renderer, const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
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

void SysRenderImpl::start()
{
  m_renderer.start();
}

double SysRenderImpl::frameRate() const
{
  return m_renderer.frameRate();
}

MeshHandle SysRenderImpl::createMesh(const CRender& c) const
{
  return m_renderer.addMesh(quad(c.size, c.textureRect));
}

void SysRenderImpl::addEntity(EntityId entityId, const CRender& data)
{
  auto mesh = createMesh(data);

  m_data.push_back(CRenderData{
    .pos = data.pos,
    .zIndex = data.zIndex,
    .mesh = mesh
  });

  m_ids.push_back(entityId);

  assert(m_data.size() == m_ids.size());

  if (entityId + 1 > m_lookup.size()) {
    m_lookup.resize(entityId + 1, 0);
  }
  m_lookup[entityId] = m_data.size() - 1;
}

void SysRenderImpl::removeEntity(EntityId entityId)
{
  if (m_data.empty()) {
    return;
  }

  auto idx = m_lookup[entityId];
  auto last = m_data.size() - 1;

  auto lastId = m_ids[last];

  std::swap(m_data[idx], m_data[last]);
  std::swap(m_ids[idx], m_ids[idx]);
  m_data.pop_back();
  m_ids.pop_back();

  assert(m_data.size() == m_ids.size());

  m_lookup[entityId] = 0;
  m_lookup[lastId] = idx;
}

bool SysRenderImpl::hasEntity(EntityId entityId) const
{
  return m_lookup.size() + 1 > entityId && m_lookup[entityId] != 0;
}

void SysRenderImpl::moveEntity(EntityId entityId, const Vec2f& pos)
{
  // TODO
}

void SysRenderImpl::playAnimation(EntityId entityId, HashedString name)
{
  // TODO
}

Camera& SysRenderImpl::camera()
{
  return m_camera;
}

const Camera& SysRenderImpl::camera() const
{
  return m_camera;
}

void SysRenderImpl::update()
{
  try {
    m_renderer.beginFrame();
    m_renderer.beginPass(render::RenderPass::Overlay, m_camera.getPosition(), m_camera.getMatrix());

    for (auto& item : m_data) {
      auto t = translationMatrix4x4(Vec3f{ item.pos[0], item.pos[1], 0.f });
      auto screenSpaceTransform = screenToWorld(t, m_renderer.getViewParams().aspectRatio);
      m_renderer.setOrderKey(item.zIndex);
      m_renderer.drawModel(item.mesh, m_textureAtlas, screenSpaceTransform);
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

SysRenderPtr createSysRender(Renderer& renderer, const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<SysRenderImpl>(renderer, fileSystem, logger);
}
