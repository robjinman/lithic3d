#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/trace.hpp"
#include "lithic3d/logger.hpp"

namespace lithic3d
{

using render::MeshPtr;
using render::MaterialPtr;

namespace
{

class RenderResourceLoaderImpl : public RenderResourceLoader
{
  public:
    RenderResourceLoaderImpl(ResourceManager& resourceManager, FileSystem& fileSystem,
      render::Renderer& renderer, Logger& logger);

    ResourceHandle loadTextureAsync(const std::filesystem::path& path) override;
    ResourceHandle loadMaterialAsync(render::MaterialPtr material) override;
    ResourceHandle loadMeshAsync(render::MeshPtr mesh) override;

    ~RenderResourceLoaderImpl() override;

  private:
    Logger& m_logger;
    ResourceManager& m_resourceManager;
    FileSystem& m_fileSystem;
    render::Renderer& m_renderer;
};

RenderResourceLoaderImpl::RenderResourceLoaderImpl(ResourceManager& resourceManager,
  FileSystem& fileSystem, render::Renderer& renderer, Logger& logger)
  : m_logger(logger)
  , m_resourceManager(resourceManager)
  , m_fileSystem(fileSystem)
  , m_renderer(renderer)
{}

ResourceHandle RenderResourceLoaderImpl::loadTextureAsync(const std::filesystem::path& path)
{
  DBG_TRACE(m_logger);

  return m_resourceManager.loadResource([this, path](ResourceId id) {
    DBG_TRACE(m_logger);

    auto data = m_fileSystem.readAppDataFile(path);
    auto texture = render::loadTexture(data);
    m_renderer.addTexture(id, std::move(texture));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeTexture(id);
      }
    };
  });
}

ResourceHandle RenderResourceLoaderImpl::loadMaterialAsync(MaterialPtr material)
{
  DBG_TRACE(m_logger);

  return m_resourceManager.loadResource([this, m = std::move(material)](ResourceId id) mutable {
    DBG_TRACE(m_logger);

    m_renderer.addMaterial(id, std::move(m));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeMaterial(id);
      }
    };
  });
}

ResourceHandle RenderResourceLoaderImpl::loadMeshAsync(MeshPtr mesh)
{
  DBG_TRACE(m_logger);

  return m_resourceManager.loadResource([this, m = std::move(mesh)](ResourceId id) mutable {
    DBG_TRACE(m_logger);

    m_renderer.addMesh(id, std::move(m));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeMesh(id);
      }
    };
  });
}

RenderResourceLoaderImpl::~RenderResourceLoaderImpl()
{
  DBG_TRACE(m_logger);
  m_resourceManager.waitAll();
}

} // namespace

RenderResourceLoaderPtr createRenderResourceLoader(ResourceManager& resourceManager,
  FileSystem& fileSystem, render::Renderer& renderer, Logger& logger)
{
  return std::make_unique<RenderResourceLoaderImpl>(resourceManager, fileSystem, renderer, logger);
}

} // lithic3d
