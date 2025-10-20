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
#include <cstring>

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

Mat4x4f screenToWorld(const Mat4x4f& transform, float aspect)
{
  Mat4x4f M = translationMatrix4x4(Vec3f{ -0.5f * aspect, -0.5f, 0.f });
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
  mesh->featureSet.flags.set(MeshFeatures::IsQuad);
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

MeshPtr textItemMesh(const std::string& text, size_t length, const Rectf& uvRect, bool dynamic)
{
  uint32_t glyphsPerRow = 12; // TODO: Don't hard-code
  char firstGlyph = ' ';
  char lastGlyph = '~';
  uint32_t numGlyphs = lastGlyph - firstGlyph + 1;
  uint32_t numRows = static_cast<uint32_t>(static_cast<float>(numGlyphs) / glyphsPerRow + 0.5f);

  float glyphW = uvRect.w / glyphsPerRow;
  float glyphH = uvRect.h / numRows;

  std::vector<Vec3f> positions;
  std::vector<Vec3f> normals;
  std::vector<Vec2f> uVs;
  std::vector<uint16_t> indices;

  for (int i = 0; i < length; ++i) {
    float a = i;
    float b = i + 1;

    positions.push_back({ a, 0, 0 });
    positions.push_back({ b, 0, 0 });
    positions.push_back({ b, 1, 0 });
    positions.push_back({ a, 1, 0 });

    normals.push_back({ 0, 0, 1 });
    normals.push_back({ 0, 0, 1 });
    normals.push_back({ 0, 0, 1 });
    normals.push_back({ 0, 0, 1 });

    int codePoint = i < text.size() ? text[i] : ' ';
    int c = codePoint - firstGlyph;
    int row = c / glyphsPerRow;
    int col = c % glyphsPerRow;
    float x0 = uvRect.x + glyphW * col;
    float x1 = x0 + glyphW;
    float y0 = uvRect.y + glyphH * row;
    float y1 = y0 + glyphH;

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
  mesh->featureSet.flags.set(MeshFeatures::IsDynamicText, dynamic);
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

    void addEntity(EntityId entityId, const SpriteData& data) override;
    void addEntity(EntityId entityId, const TextData& data) override;
    void addEntity(EntityId entityId, const DynamicTextData& data) override;
    void addEntity(EntityId entityId, const QuadData& data) override;

    void addScissor(ScissorId id, const Recti& scissor) override;
    void setClearColour(const Vec4f& colour) override;
    void onResize() override;

    void setZIndex(EntityId entityId, uint32_t zIndex) override;
    void setTextureRect(EntityId entityId, const Rectf& textureRect) override;
    void setVisible(EntityId entityId, bool visible) override;
    void setColour(EntityId entityId, const Vec4f& colour) override;
    void updateDynamicText(EntityId entityId, const std::string& text) override;

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
    std::unordered_map<ScissorId, Recti> m_scissors;
    Vec4f m_clearColour{ 0.f, 0.f, 0.f, 1.f };
};

SysRenderImpl::SysRenderImpl(ComponentStore& componentStore, Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_componentStore(componentStore)
  , m_renderer(renderer)
  , m_fileSystem(fileSystem)
{
  // Text
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
  }

  // Sprites
  {
    MeshFeatureSet meshFeatures{
      .vertexLayout = {
        BufferUsage::AttrPosition,
        BufferUsage::AttrNormal,
        BufferUsage::AttrTexCoord
      },
      .flags{}
    };
    meshFeatures.flags.set(MeshFeatures::IsQuad);
    MaterialFeatureSet materialFeatures{};
    materialFeatures.flags.set(MaterialFeatures::HasTexture);
    m_renderer.compileShader(meshFeatures, materialFeatures);
  }

  // Quads
  {
    MeshFeatureSet meshFeatures{
      .vertexLayout = {
        BufferUsage::AttrPosition,
        BufferUsage::AttrNormal,
        BufferUsage::AttrTexCoord
      },
      .flags{}
    };
    meshFeatures.flags.set(MeshFeatures::IsQuad);
    MaterialFeatureSet materialFeatures{};
    m_renderer.compileShader(meshFeatures, materialFeatures);
  }

  // Dynamic text
  {
    MeshFeatureSet meshFeatures{
      .vertexLayout = {
        BufferUsage::AttrPosition,
        BufferUsage::AttrNormal,
        BufferUsage::AttrTexCoord
      },
      .flags{}
    };
    meshFeatures.flags.set(MeshFeatures::IsDynamicText);
    MaterialFeatureSet materialFeatures{};
    materialFeatures.flags.set(MaterialFeatures::HasTexture);
    m_renderer.compileShader(meshFeatures, materialFeatures);
  }

  auto atlas = render::loadTexture(m_fileSystem.readAppDataFile("textures/atlas.png"));

  MaterialFeatureSet atlasFeatures{};
  atlasFeatures.flags.set(MaterialFeatures::HasTexture);

  auto atlasMaterial = std::make_unique<Material>(atlasFeatures);
  atlasMaterial->texture.id = m_renderer.addTexture(std::move(atlas));

  m_textureAtlas = m_renderer.addMaterial(std::move(atlasMaterial));

  m_mesh = m_renderer.addMesh(quad());
}

void SysRenderImpl::start()
{
  m_renderer.start();
}

double SysRenderImpl::frameRate() const
{
  return m_renderer.frameRate();
}

void SysRenderImpl::addScissor(ScissorId id, const Recti& scissor)
{
  ASSERT(id != 0, "Scissor ID 0 is reserved; choose a different number");

  m_scissors[id] = scissor;
}

void SysRenderImpl::setClearColour(const Vec4f& colour)
{
  m_clearColour = colour;
}

void SysRenderImpl::onResize()
{
  m_renderer.onResize();  // Async
}

void SysRenderImpl::addEntity(EntityId entityId, const TextData& data)
{
  ASSERT(!data.text.empty(), "Text must not be empty");
  ASSERT(data.scissor != 0, "Scissor ID 0 is reserved; choose a different number");

  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender>(m_componentStore, entityId);
  assertHasComponent<CSprite>(m_componentStore, entityId);

  m_componentStore.component<CRender>(entityId) = CRender{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };

  m_componentStore.component<CSprite>(entityId) = CSprite{
    .textureRect = data.textureRect,
    .isText = true
  };

  auto mesh = textItemMesh(data.text, data.text.size(), data.textureRect, false);

  MeshHandle meshHandle;
  if (m_renderer.isStarted()) {
    // TODO: This blocks
    meshHandle = m_renderer.addMeshAsync(std::move(mesh)).value<render::AddMeshResult>().handle;
  }
  else {
    meshHandle = m_renderer.addMesh(std::move(mesh));
  }

  m_textItems.insert({ entityId, meshHandle });
}

void SysRenderImpl::addEntity(EntityId entityId, const DynamicTextData& data)
{
  ASSERT(!data.text.empty(), "Text must not be empty");
  ASSERT(data.scissor != 0, "Scissor ID 0 is reserved; choose a different number");

  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender>(m_componentStore, entityId);
  assertHasComponent<CSprite>(m_componentStore, entityId);
  assertHasComponent<CDynamicText>(m_componentStore, entityId);

  m_componentStore.component<CRender>(entityId) = CRender{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };

  m_componentStore.component<CSprite>(entityId) = CSprite{
    .textureRect = data.textureRect,
    .isText = true
  };

  auto& c = m_componentStore.component<CDynamicText>(entityId);
  size_t len = data.text.length();
  ASSERT(len <= DYNAMIC_TEXT_MAX_LEN, "Dynamic text length must not exceed "
    << DYNAMIC_TEXT_MAX_LEN << " bytes");

  memcpy(c.text, data.text.data(), len);
  c.text[len] = '\0';

  auto mesh = textItemMesh(data.text, data.maxLength, data.textureRect, true);

  MeshHandle meshHandle;
  if (m_renderer.isStarted()) {
    // TODO: This blocks
    meshHandle = m_renderer.addMeshAsync(std::move(mesh)).value<render::AddMeshResult>().handle;
  }
  else {
    meshHandle = m_renderer.addMesh(std::move(mesh));
  }

  m_textItems.insert({ entityId, meshHandle });
}

void SysRenderImpl::addEntity(EntityId entityId, const QuadData& data)
{
  ASSERT(data.scissor != 0, "Scissor ID 0 is reserved; choose a different number");

  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender>(m_componentStore, entityId);

  m_componentStore.component<CRender>(entityId) = CRender{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };
}

void SysRenderImpl::addEntity(EntityId entityId, const SpriteData& data)
{
  ASSERT(data.scissor != 0, "Scissor ID 0 is reserved; choose a different number");

  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender>(m_componentStore, entityId);
  assertHasComponent<CSprite>(m_componentStore, entityId);

  m_componentStore.component<CRender>(entityId) = CRender{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };

  m_componentStore.component<CSprite>(entityId) = CSprite{
    .textureRect = data.textureRect,
    .isText = false
  };
}

void SysRenderImpl::setZIndex(EntityId entityId, uint32_t zIndex)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  renderComp.zIndex = zIndex;
}

void SysRenderImpl::setTextureRect(EntityId entityId, const Rectf& textureRect)
{
  auto& renderComp = m_componentStore.component<CSprite>(entityId);
  renderComp.textureRect = textureRect;
}

void SysRenderImpl::setVisible(EntityId entityId, bool visible)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  renderComp.visible = visible;
}

void SysRenderImpl::setColour(EntityId entityId, const Vec4f& colour)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  renderComp.colour = colour;
}

void SysRenderImpl::updateDynamicText(EntityId entityId, const std::string& text)
{
  ASSERT(text.size() <= DYNAMIC_TEXT_MAX_LEN, "Dynamic text exceeds maximum");

  char* buffer = m_componentStore.component<CDynamicText>(entityId).text;
  memset(buffer, '\0', DYNAMIC_TEXT_MAX_LEN);
  memcpy(buffer, text.data(), text.size());
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
    auto screenAspect = m_renderer.getViewParams().aspectRatio;
    float gameAspect = 630.f / 480.f;  // TODO
    m_camera.setPosition(Vec3f{ -0.5f * (screenAspect - gameAspect), 0.f, 1.f });

    m_renderer.beginFrame(m_clearColour);
    m_renderer.beginPass(render::RenderPass::Overlay, m_camera.getPosition(), m_camera.getMatrix());

    ScissorId scissor = 0;
    for (auto& group : m_componentStore.components<CRender>()) {
      size_t n = group.numEntities();
      auto renderComps = group.components<CRender>();
      auto spriteComps = group.components<CSprite>();
      auto globalTs = group.components<CGlobalTransform>();
      auto flags = group.components<CSpatialFlags>();
      auto dynamicTextComps = group.components<CDynamicText>();
      auto& entityIds = group.entityIds();

      for (size_t i = 0; i < n; ++i) {
        auto& renderComp = renderComps[i];

        if (!renderComp.visible) {
          continue;
        }
        if (!(flags[i].enabled && flags[i].parentEnabled)) {
          continue;
        }

        auto& t = globalTs[i].transform;
        auto screenSpaceTransform = screenToWorld(t, m_renderer.getViewParams().aspectRatio);

        m_renderer.setOrderKey(renderComp.zIndex);

        if (renderComp.scissor != scissor) {
          scissor = renderComp.scissor;
          m_renderer.setScissor(m_scissors.at(scissor));
        }

        if (!spriteComps.empty()) {
          auto& spriteComp = spriteComps[i];

          if (spriteComp.isText) {
            if (!dynamicTextComps.empty()) {
              auto& mesh = m_textItems.at(entityIds[i]);
              m_renderer.drawDynamicText(mesh, m_textureAtlas, dynamicTextComps[i].text,
                renderComp.colour, screenSpaceTransform);
            }
            else {
              auto& mesh = m_textItems.at(entityIds[i]);
              m_renderer.drawModel(mesh, m_textureAtlas, screenSpaceTransform, renderComp.colour);
            }
          }
          else {
            Rectf& r = spriteComp.textureRect;

            std::array<Vec2f, 4> uvCoords{
              Vec2f{ r.x, r.y + r.h },
              Vec2f{ r.x + r.w, r.y + r.h },
              Vec2f{ r.x + r.w, r.y },
              Vec2f{ r.x, r.y }
            };

            m_renderer.drawSprite(m_mesh, m_textureAtlas, uvCoords, renderComp.colour,
              screenSpaceTransform);
          }
        }
        else {
          m_renderer.drawQuad(m_mesh, renderComp.colour, screenSpaceTransform);
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
