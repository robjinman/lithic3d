#include "lithic3d/sys_render_2d.hpp"
#include "lithic3d/renderer.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/camera_2d.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/time.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/trace.hpp"
#include <cassert>
#include <functional>
#include <map>
#include <cstring>

namespace lithic3d
{

using render::Renderer;
using render::Mesh;
using render::MeshPtr;
using render::Material;
using render::MeshFeatureSet;
using render::MaterialFeatureSet;
namespace MaterialFeatures = render::MaterialFeatures;
using render::MaterialHandle;
namespace MeshFeatures = render::MeshFeatures;
using render::MeshHandle;
using render::RenderPass;
using render::Buffer;
using render::BufferUsage;
using render::AlignedBytes;

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
  MeshPtr mesh = std::make_unique<Mesh>();
  mesh->featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };
  mesh->featureSet.flags.set(MeshFeatures::IsQuad);
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec3f>{
      { 0, 0, 0 },
      { 1, 0, 0 },
      { 1, 1, 0 },
      { 0, 1, 0 }
    }}, BufferUsage::AttrPosition
  });
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec3f>{
      { 0, 0, 1 },
      { 0, 0, 1 },
      { 0, 0, 1 },
      { 0, 0, 1 }
    }}, BufferUsage::AttrNormal
  });
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec2f>{
      { 0, 1 },
      { 1, 1 },
      { 1, 0 },
      { 0, 0 }
    }}, BufferUsage::AttrTexCoord
  });
  mesh->indexBuffer = Buffer{AlignedBytes{
    std::vector<uint16_t>{
      0, 1, 2, 0, 2, 3
    }},
    BufferUsage::Index
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
  std::vector<Vec2f> uvs;
  std::vector<uint16_t> indices;

  for (size_t i = 0; i < length; ++i) {
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

    uvs.push_back({ x0, y1 });
    uvs.push_back({ x1, y1 });
    uvs.push_back({ x1, y0 });
    uvs.push_back({ x0, y0 });

    indices.insert(indices.end(), {
      static_cast<uint16_t>(i * 4),
      static_cast<uint16_t>(i * 4 + 1),
      static_cast<uint16_t>(i * 4 + 2),
      static_cast<uint16_t>(i * 4),
      static_cast<uint16_t>(i * 4 + 2),
      static_cast<uint16_t>(i * 4 + 3)
    });
  }

  MeshPtr mesh = std::make_unique<Mesh>();
  mesh->featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };
  mesh->featureSet.flags.set(MeshFeatures::IsDynamicText, dynamic);
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{positions}, BufferUsage::AttrPosition});
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{normals}, BufferUsage::AttrNormal});
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{uvs}, BufferUsage::AttrTexCoord});
  mesh->indexBuffer = Buffer{AlignedBytes{indices}, BufferUsage::Index};

  return mesh;
}

class SysRender2dImpl : public SysRender2d
{
  public:
    SysRender2dImpl(ComponentStore& componentStore, Renderer& renderer,
      RenderResourceLoader& renderResourceLoader, Logger& logger);

    double frameRate() const override;

    Camera2d& camera() override;
    const Camera2d& camera() const override;

    render::Renderer& renderer() override;

    void addEntity(EntityId entityId, const DSprite& data) override;
    void addEntity(EntityId entityId, const DText& data) override;
    void addEntity(EntityId entityId, const DDynamicText& data) override;
    void addEntity(EntityId entityId, const DQuad& data) override;

    void addScissor(ScissorId id, const Recti& scissor) override;

    void setZIndex(EntityId entityId, uint32_t zIndex) override;
    void setTextureRect(EntityId entityId, const Rectf& textureRect) override;
    void setVisible(EntityId entityId, bool visible) override;
    void setColour(EntityId entityId, const Vec4f& colour) override;
    void updateDynamicText(EntityId entityId, const std::string& text) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event&) override {}

    ~SysRender2dImpl() override;

  private:
    Logger& m_logger;
    ComponentStore& m_componentStore;
    std::unique_ptr<Camera2d> m_camera;
    Renderer& m_renderer;
    RenderResourceLoader& m_renderResourceLoader;
    MeshHandle m_mesh;
    std::unordered_map<EntityId, MeshHandle> m_textItemMeshHandles;
    std::unordered_map<EntityId, MaterialHandle> m_materialHandles;
    std::unordered_map<ScissorId, Recti> m_scissors;
};

SysRender2dImpl::SysRender2dImpl(ComponentStore& componentStore, Renderer& renderer,
  RenderResourceLoader& renderResourceLoader, Logger& logger)
  : m_logger(logger)
  , m_componentStore(componentStore)
  , m_renderer(renderer)
  , m_renderResourceLoader(renderResourceLoader)
{
  auto viewport = renderer.getViewportSize();
  float aspectRatio = static_cast<float>(viewport[0]) / viewport[1];
  m_camera = std::make_unique<Camera2d>(aspectRatio, renderer.getViewportRotation());
  m_mesh = m_renderResourceLoader.loadMeshAsync(quad()).wait();
}

render::Renderer& SysRender2dImpl::renderer()
{
  return m_renderer;
}

double SysRender2dImpl::frameRate() const
{
  return m_renderer.frameRate();
}

void SysRender2dImpl::addScissor(ScissorId id, const Recti& scissor)
{
  ASSERT(id != 0, "Scissor ID 0 is reserved; choose a different number");

  m_scissors[id] = scissor;
}

void SysRender2dImpl::addEntity(EntityId entityId, const DText& data)
{
  ASSERT(!data.text.empty(), "Text must not be empty");

  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender2d>(m_componentStore, entityId);
  assertHasComponent<CSprite>(m_componentStore, entityId);
  assertHasComponent<CMesh2d>(m_componentStore, entityId);
  assertHasComponent<CMaterial2d>(m_componentStore, entityId);

  m_componentStore.instantiate<CRender2d>(entityId) = CRender2d{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };

  // TODO: Assert material features are supported?

  m_componentStore.instantiate<CSprite>(entityId) = CSprite{
    .textureRect = data.textureRect,
    .isText = true
  };

  auto mesh = textItemMesh(data.text, data.text.size(), data.textureRect, false);
  auto meshHandle = m_renderResourceLoader.loadMeshAsync(std::move(mesh));

  m_componentStore.instantiate<CMesh2d>(entityId) = CMesh2d{
    .id = meshHandle.resource.id(),
    .features = meshHandle.features
  };

  m_componentStore.instantiate<CMaterial2d>(entityId) = CMaterial2d{
    .id = data.material.resource.id(),
    .features = data.material.features
  };

  m_textItemMeshHandles.insert({ entityId, meshHandle });
  m_materialHandles.insert({ entityId, data.material });
}

void SysRender2dImpl::addEntity(EntityId entityId, const DDynamicText& data)
{
  ASSERT(!data.text.empty(), "Text must not be empty");

  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender2d>(m_componentStore, entityId);
  assertHasComponent<CSprite>(m_componentStore, entityId);
  assertHasComponent<CDynamicText>(m_componentStore, entityId);
  assertHasComponent<CMesh2d>(m_componentStore, entityId);
  assertHasComponent<CMaterial2d>(m_componentStore, entityId);

  m_componentStore.instantiate<CRender2d>(entityId) = CRender2d{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };

  m_componentStore.instantiate<CSprite>(entityId) = CSprite{
    .textureRect = data.textureRect,
    .isText = true
  };

  auto& c = m_componentStore.instantiate<CDynamicText>(entityId);
  size_t len = data.text.length();
  ASSERT(len <= DYNAMIC_TEXT_MAX_LEN, "Dynamic text length must not exceed "
    << DYNAMIC_TEXT_MAX_LEN << " bytes");

  memcpy(c.text, data.text.data(), len);
  c.text[len] = '\0';

  auto mesh = textItemMesh(data.text, data.maxLength, data.textureRect, true);
  MeshFeatureSet meshFeatures = mesh->featureSet;

  auto meshHandle = m_renderResourceLoader.loadMeshAsync(std::move(mesh));

  m_componentStore.instantiate<CMesh2d>(entityId) = CMesh2d{
    .id = meshHandle.resource.id(),
    .features = meshFeatures
  };

  m_componentStore.instantiate<CMaterial2d>(entityId) = CMaterial2d{
    .id = data.material.resource.id(),
    .features = data.material.features
  };

  m_textItemMeshHandles.insert({ entityId, meshHandle });
  m_materialHandles.insert({ entityId, data.material });
}

void SysRender2dImpl::addEntity(EntityId entityId, const DQuad& data)
{
  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender2d>(m_componentStore, entityId);
  assertHasComponent<CQuad>(m_componentStore, entityId);

  m_componentStore.instantiate<CRender2d>(entityId) = CRender2d{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };

  m_componentStore.instantiate<CQuad>(entityId) = CQuad{
    .radius = data.radius
  };
}

void SysRender2dImpl::addEntity(EntityId entityId, const DSprite& data)
{
  assertHasComponent<CGlobalTransform>(m_componentStore, entityId);
  assertHasComponent<CSpatialFlags>(m_componentStore, entityId);
  assertHasComponent<CRender2d>(m_componentStore, entityId);
  assertHasComponent<CSprite>(m_componentStore, entityId);
  assertHasComponent<CMaterial2d>(m_componentStore, entityId);

  m_componentStore.instantiate<CRender2d>(entityId) = CRender2d{
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true,
    .scissor = data.scissor
  };

  m_componentStore.instantiate<CSprite>(entityId) = CSprite{
    .textureRect = data.textureRect,
    .isText = false
  };

  m_componentStore.instantiate<CMaterial2d>(entityId) = CMaterial2d{
    .id = data.material.resource.id(),
    .features = data.material.features
  };

  m_materialHandles.insert({ entityId, data.material });
}

void SysRender2dImpl::setZIndex(EntityId entityId, uint32_t zIndex)
{
  auto& renderComp = m_componentStore.component<CRender2d>(entityId);
  renderComp.zIndex = zIndex;
}

void SysRender2dImpl::setTextureRect(EntityId entityId, const Rectf& textureRect)
{
  auto& renderComp = m_componentStore.component<CSprite>(entityId);
  renderComp.textureRect = textureRect;
}

void SysRender2dImpl::setVisible(EntityId entityId, bool visible)
{
  auto& renderComp = m_componentStore.component<CRender2d>(entityId);
  renderComp.visible = visible;
}

void SysRender2dImpl::setColour(EntityId entityId, const Vec4f& colour)
{
  auto& renderComp = m_componentStore.component<CRender2d>(entityId);
  renderComp.colour = colour;
}

void SysRender2dImpl::updateDynamicText(EntityId entityId, const std::string& text)
{
  ASSERT(text.size() <= DYNAMIC_TEXT_MAX_LEN, "Dynamic text exceeds maximum");

  char* buffer = m_componentStore.component<CDynamicText>(entityId).text;
  memset(buffer, '\0', DYNAMIC_TEXT_MAX_LEN);
  memcpy(buffer, text.data(), text.size());
}

void SysRender2dImpl::removeEntity(EntityId entityId)
{
  m_textItemMeshHandles.erase(entityId);
  m_materialHandles.erase(entityId);
}

bool SysRender2dImpl::hasEntity(EntityId entityId) const
{
  return m_componentStore.hasComponentForEntity<CSprite>(entityId);
}

Camera2d& SysRender2dImpl::camera()
{
  return *m_camera;
}

const Camera2d& SysRender2dImpl::camera() const
{
  return *m_camera;
}

void SysRender2dImpl::update(Tick, const InputState&)
{
  auto viewport = m_renderer.getViewportSize();
  float screenAspect = static_cast<float>(viewport[0]) / viewport[1];
  float gameAspect = 630.f / 480.f;  // TODO
  m_camera->setPosition(Vec3f{ -0.5f * (screenAspect - gameAspect), 0.f, 1.f });

  m_renderer.beginPass(render::RenderPass::Overlay, m_camera->getPosition(),
    m_camera->getViewMatrix(), m_camera->getProjectionMatrix());

  ScissorId scissor = std::numeric_limits<ScissorId>::max();
  for (auto& group : m_componentStore.components<CRender2d>()) {
    size_t n = group.numEntities();
    auto renderComps = group.components<CRender2d>();
    auto spriteComps = group.components<CSprite>();
    auto globalTs = group.components<CGlobalTransform>();
    auto flagsComps = group.components<CSpatialFlags>();
    auto dynamicTextComps = group.components<CDynamicText>();
    auto quadComps = group.components<CQuad>();
    auto meshComps = group.components<CMesh2d>();
    auto materialComps = group.components<CMaterial2d>();
    auto& entityIds = group.entityIds();

    for (size_t i = 0; i < n; ++i) {
      auto& renderComp = renderComps[i];
      auto& flags = flagsComps[i].flags;

      if (!renderComp.visible) {
        continue;
      }
      if (!(flags.test(SpatialFlags::Enabled) && flags.test(SpatialFlags::ParentEnabled))) {
        continue;
      }

      auto& t = globalTs[i].transform;
      auto screenSpaceTransform = screenToWorld(t, screenAspect);

      m_renderer.setOrderKey(renderComp.zIndex);

      if (renderComp.scissor != scissor) {
        scissor = renderComp.scissor;

        if (scissor != 0) {
          m_renderer.setScissor(m_scissors.at(scissor));
        }
      }

      if (!spriteComps.empty()) {
        auto& spriteComp = spriteComps[i];
        auto& materialComp = materialComps[i];

        if (spriteComp.isText) {
          auto& meshComp = meshComps[i];

          auto& textItemMesh = m_textItemMeshHandles.at(entityIds[i]); // TODO: slow?
          if (!textItemMesh.resource.ready()) {
            continue;
          }

          if (!dynamicTextComps.empty()) {
            m_renderer.drawDynamicText(meshComp.id, meshComp.features, materialComp.id,
              materialComp.features, dynamicTextComps[i].text, renderComp.colour,
              screenSpaceTransform);
          }
          else {
            m_renderer.drawModel(meshComp.id, meshComp.features, materialComp.id,
              materialComp.features, renderComp.colour, screenSpaceTransform);
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

          m_renderer.drawSprite(m_mesh.resource.id(), m_mesh.features, materialComp.id,
            materialComp.features, uvCoords, renderComp.colour, screenSpaceTransform);
        }
      }
      else {
        m_renderer.drawQuad(m_mesh.resource.id(), m_mesh.features, quadComps[i].radius,
          renderComp.colour, screenSpaceTransform);
      }
    }
  }

  m_renderer.endPass();
}

SysRender2dImpl::~SysRender2dImpl()
{
  DBG_TRACE(m_logger);
}

} // namespace

SysRender2dPtr createSysRender2d(ComponentStore& componentStore, Renderer& renderer,
  RenderResourceLoader& renderResourceLoader, Logger& logger)
{
  return std::make_unique<SysRender2dImpl>(componentStore, renderer, renderResourceLoader, logger);
}

} // namespace lithic3d
