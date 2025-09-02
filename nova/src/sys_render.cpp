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

Mat3x3f screenToWorld(const Mat3x3f& transform, float_t aspect)
{
  static Mat3x3f M = ([aspect]() {
    auto m = identityMatrix<float_t, 3>();
    m.set(0, 2, -0.5 * aspect);
    m.set(1, 2, -0.5 * aspect);
    m.set(2, 2, 1.f);
    return m;
  })();

  return M * transform;
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
    void addEntity(EntityId entityId, const TextItemData& data) override;
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
    std::unordered_map<EntityId, TextItemData> m_textComponents;
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
  meshFeatures.flags.set(MeshFeatures::IsInstanced);
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

void SysRenderImpl::addEntity(EntityId entityId, const SpriteData& data)
{
  m_componentStore.component<CRender>(entityId) = CRender{
    .textureRect = data.textureRect,
    .colour = data.colour,
    .zIndex = data.zIndex,
    .visible = true
  };
}

void SysRenderImpl::addEntity(EntityId entityId, const TextItemData& data)
{
  m_textComponents.insert({ entityId, data });
}

void SysRenderImpl::setZIndex(EntityId entityId, uint32_t zIndex)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  renderComp.zIndex = zIndex;
}

void SysRenderImpl::setTextureRect(EntityId entityId, const Rectf& textureRect)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  renderComp.textureRect = textureRect;
}

void SysRenderImpl::setVisible(EntityId entityId, bool visible)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  renderComp.visible = visible;
}

void SysRenderImpl::removeEntity(EntityId entityId)
{
  m_textComponents.erase(entityId);
}

bool SysRenderImpl::hasEntity(EntityId entityId) const
{
  return m_componentStore.hasComponentForEntity<CRender>(entityId);
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

    for (auto& group : m_componentStore.components<CRender>()) {
      size_t n = group.numEntities();
      auto renderComps = group.components<CRender>();
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
        Mat3x3f transform{
          t.at(0, 0), t.at(0, 1), t.at(0, 3),
          t.at(1, 0), t.at(1, 1), t.at(1, 3),
          t.at(2, 0), t.at(2, 1), t.at(2, 3),
        };
        auto worldTransform = screenToWorld(transform, m_renderer.getViewParams().aspectRatio);

        m_renderer.setOrderKey(item.zIndex);
        m_renderer.drawSprite(m_textureAtlas, item.textureRect, item.colour, worldTransform);
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
