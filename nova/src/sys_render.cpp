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

// TODO: Remove normals
MeshPtr quad()
{
  MeshPtr mesh = std::make_unique<Mesh>(MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  });
  mesh->featureSet.flags.set(MeshFeatures::IsSprite);
  mesh->attributeBuffers = {
    Buffer{
      .usage = BufferUsage::AttrPosition,
      .data = toBytes(std::vector<Vec3f>{
        { 0, 0, 0 },
        { 1, 0, 0 },
        { 1, 1, 0 },
        { 0, 1, 0 }
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
        { 0, 1 },
        { 1, 1 },
        { 1, 0 },
        { 0, 0 }
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

MeshPtr textItemMesh(const std::string& text, const Rectf& uvRect)
{
  uint32_t glyphsPerRow = 12; // TODO: Don't hard-code
  char firstGlyph = ' ';
  char lastGlyph = '~';
  uint32_t numGlyphs = lastGlyph - firstGlyph + 1;
  uint32_t numRows = static_cast<uint32_t>(static_cast<float>(numGlyphs) / glyphsPerRow + 0.5f);

  float_t glyphW = uvRect.w / glyphsPerRow;
  float_t glyphH = uvRect.h / numRows;

  std::vector<Vec3f> positions;
  std::vector<Vec3f> normals;
  std::vector<Vec2f> uVs;
  std::vector<uint16_t> indices;

  for (int i = 0; i < text.size(); ++i) {
    float_t a = i;
    float_t b = i + 1;

    positions.push_back({ a, 0, 0 });
    positions.push_back({ b, 0, 0 });
    positions.push_back({ b, 1, 0 });
    positions.push_back({ a, 1, 0 });

    normals.push_back({ 0, 0, 1 });
    normals.push_back({ 0, 0, 1 });
    normals.push_back({ 0, 0, 1 });
    normals.push_back({ 0, 0, 1 });

    int c = text[i] - firstGlyph;
    int row = c / glyphsPerRow;
    int col = c % glyphsPerRow;
    float_t x0 = uvRect.x + glyphW * col;
    float_t x1 = x0 + glyphW;
    float_t y0 = uvRect.y + glyphH * row;
    float_t y1 = y0 + glyphH;

    uVs.push_back({ x0, y1 });
    uVs.push_back({ x1, y1 });
    uVs.push_back({ x1, y0 });
    uVs.push_back({ x0, y0 });

    indices.insert(indices.end(), {
      static_cast<uint16_t>(i * 4),
      static_cast<uint16_t>(i * 4 + 1),
      static_cast<uint16_t>(i * 4 + 2),
      static_cast<uint16_t>(i * 4),
      static_cast<uint16_t>(i * 4 + 2),
      static_cast<uint16_t>(i * 4 + 3)
    });
  }

  MeshPtr mesh = std::make_unique<Mesh>(MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  });
  mesh->attributeBuffers = {
    Buffer{
      .usage = BufferUsage::AttrPosition,
      .data = toBytes(positions)
    },
    Buffer{
      .usage = BufferUsage::AttrNormal,
      .data = toBytes(normals)
    },
    Buffer{
      .usage = BufferUsage::AttrTexCoord,
      .data = toBytes(uVs)
    }
  };
  mesh->indexBuffer = Buffer{
    .usage = BufferUsage::Index,
    .data = toBytes(indices)
  };

  return mesh;
}

class SysRenderImpl : public SysRender
{
  public:
    SysRenderImpl(ComponentStore& componentStore, Renderer& renderer, const FileSystem& fileSystem,
      Logger& logger);

    void start() override;
    double frameRate() const override;

    Camera& camera() override;
    const Camera& camera() const override;

    void addEntity(EntityId entityId, const SpriteData& component) override;
    void setZIndex(EntityId entityId, uint32_t zIndex) override;
    void setTextureRect(EntityId entityId, const Rectf& textureRect) override;
    void setVisible(EntityId entityId, bool visible) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override {}

  private:
    Logger& m_logger;
    ComponentStore& m_componentStore;
    Camera m_camera;
    Renderer& m_renderer;
    const FileSystem& m_fileSystem;
    MaterialHandle m_textureAtlas;
    MeshHandle m_mesh;
    std::unordered_map<EntityId, MeshHandle> m_textItems;
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
  MaterialFeatureSet materialFeatures{};
  materialFeatures.flags.set(MaterialFeatures::HasTexture);

  m_renderer.compileShader(meshFeatures, materialFeatures);

  meshFeatures.flags.set(MeshFeatures::IsSprite);
  m_renderer.compileShader(meshFeatures, materialFeatures);

  auto texture = render::loadTexture(m_fileSystem.readFile("textures/atlas.png"));

  auto material = std::make_unique<Material>(materialFeatures);
  material->texture.id = m_renderer.addTexture(std::move(texture));

  m_textureAtlas = m_renderer.addMaterial(std::move(material));

  m_mesh = m_renderer.addMesh(quad());

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

void SysRenderImpl::addEntity(EntityId entityId, const SpriteData& data)
{
  m_componentStore.component<CSprite>(entityId) = CSprite{
    .textureRect = data.textureRect,
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .isText = !data.text.empty()
  };

  if (!data.text.empty()) {
    auto mesh = textItemMesh(data.text, data.textureRect);
    m_textItems.insert({ entityId, m_renderer.addMesh(std::move(mesh)) });
  }
}

void SysRenderImpl::setZIndex(EntityId entityId, uint32_t zIndex)
{
  auto& renderComp = m_componentStore.component<CSprite>(entityId);
  renderComp.zIndex = zIndex;
}

void SysRenderImpl::setTextureRect(EntityId entityId, const Rectf& textureRect)
{
  auto& renderComp = m_componentStore.component<CSprite>(entityId);
  renderComp.textureRect = textureRect;
}

void SysRenderImpl::setVisible(EntityId entityId, bool visible)
{
  auto& renderComp = m_componentStore.component<CSprite>(entityId);
  renderComp.visible = visible;
}

void SysRenderImpl::removeEntity(EntityId entityId)
{
  m_textItems.erase(entityId);
  // TODO: Delete meshes from renderer
}

bool SysRenderImpl::hasEntity(EntityId entityId) const
{
  return m_componentStore.hasComponentForEntity<CSprite>(entityId);
}

Camera& SysRenderImpl::camera()
{
  return m_camera;
}

const Camera& SysRenderImpl::camera() const
{
  return m_camera;
}

void SysRenderImpl::update(Tick, const InputState&)
{
  try {
    m_renderer.beginFrame({ 0.f, 0.f, 0.f, 1.f });
    m_renderer.beginPass(render::RenderPass::Overlay, m_camera.getPosition(), m_camera.getMatrix());

    for (auto& group : m_componentStore.components<CSprite>()) {
      size_t n = group.numEntities();
      auto renderComps = group.components<CSprite>();
      auto globalTs = group.components<CGlobalTransform>();
      auto flags = group.components<CSpatialFlags>();
      auto& entityIds = group.entityIds();

      for (size_t i = 0; i < n; ++i) {
        auto& item = renderComps[i];
        if (!item.visible) {
          continue;
        }
        if (!(flags[i].enabled && flags[i].parentEnabled)) {
          continue;
        }

        auto& t = globalTs[i].transform;
        auto screenSpaceTransform = screenToWorld(t, m_renderer.getViewParams().aspectRatio);

        m_renderer.setOrderKey(item.zIndex);

        if (item.isText) {
          const auto& mesh = m_textItems.at(entityIds[i]);
          m_renderer.drawModel(mesh, m_textureAtlas, screenSpaceTransform);
        }
        else {
          const Rectf& r = item.textureRect;

          std::array<Vec2f, 4> uvCoords{
            Vec2f{ r.x, r.y + r.h },
            Vec2f{ r.x + r.w, r.y + r.h },
            Vec2f{ r.x + r.w, r.y },
            Vec2f{ r.x, r.y }
          };

          m_renderer.drawSprite(m_mesh, m_textureAtlas, uvCoords, item.colour,
            screenSpaceTransform);
        }
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
