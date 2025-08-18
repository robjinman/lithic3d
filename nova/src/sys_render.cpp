#include "sys_render.hpp"
#include "renderer.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "exception.hpp"
#include "utils.hpp"
#include "time.hpp"
#include "file_system.hpp"
#include "ecs.hpp"
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

struct CRenderData
{
  Vec2f pos;
  uint32_t zIndex;
  MeshHandle mesh; // TODO: Share meshes or use instancing?

  static constexpr ComponentType TypeId = ComponentTypeId::Render;
};

static_assert(sizeof(CRenderData) == sizeof(CRenderView));

class SysRenderImpl : public SysRender
{
  public:
    SysRenderImpl(World& world, Renderer& renderer, const FileSystem& fileSystem, Logger& logger);

    void start() override;
    double frameRate() const override;

    Camera& camera() override;
    const Camera& camera() const override;

    void addEntity(EntityId entityId, const CRender& component) override;
    const Vec2f& getPosition(EntityId entityId) const override;
    void setPosition(EntityId entityId, const Vec2f& pos) override;
    void move(EntityId entityId, const Vec2f& delta) override;
    void setTextureRect(EntityId entityId, const Rectf& textureRect) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const GameEvent& event) override {}

  private:
    Logger& m_logger;
    World& m_world;
    Camera m_camera;
    Renderer& m_renderer;
    const FileSystem& m_fileSystem;
    MaterialHandle m_textureAtlas;

    MeshHandle createMesh(const CRender& c) const;
};

SysRenderImpl::SysRenderImpl(World& world, Renderer& renderer, const FileSystem& fileSystem,
  Logger& logger)
  : m_logger(logger)
  , m_world(world)
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

  m_world.component<CRenderData>(entityId) = CRenderData{
    .pos = data.pos,
    .zIndex = data.zIndex,
    .mesh = mesh
  };
}

void SysRenderImpl::removeEntity(EntityId entityId)
{
}

bool SysRenderImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

const Vec2f& SysRenderImpl::getPosition(EntityId entityId) const
{
  return m_world.component<CRenderData>(entityId).pos;
}

void SysRenderImpl::setPosition(EntityId entityId, const Vec2f& pos)
{
  m_world.component<CRenderData>(entityId).pos = pos;
}

void SysRenderImpl::move(EntityId entityId, const Vec2f& pos)
{
  m_world.component<CRenderData>(entityId).pos += pos;
}

void SysRenderImpl::setTextureRect(EntityId entityId, const Rectf& textureRect)
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

void SysRenderImpl::update(Tick tick, const InputState&)
{
  try {
    m_renderer.beginFrame();
    m_renderer.beginPass(render::RenderPass::Overlay, m_camera.getPosition(), m_camera.getMatrix());

    for (auto& group : m_world.components<CRenderData>()) {
      for (auto& item : group.components<CRenderData>()) {
        auto t = translationMatrix4x4(Vec3f{ item.pos[0], item.pos[1], 0.f });
        auto screenSpaceTransform = screenToWorld(t, m_renderer.getViewParams().aspectRatio);
        m_renderer.setOrderKey(item.zIndex);
        m_renderer.drawModel(item.mesh, m_textureAtlas, screenSpaceTransform);
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

SysRenderPtr createSysRender(World& world, Renderer& renderer, const FileSystem& fileSystem,
  Logger& logger)
{
  return std::make_unique<SysRenderImpl>(world, renderer, fileSystem, logger);
}
