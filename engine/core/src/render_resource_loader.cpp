#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/trace.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include <unordered_map>

namespace lithic3d
{

namespace fs = std::filesystem;

using render::MeshPtr;
using render::MeshHandle;
using render::MaterialPtr;
using render::MaterialHandle;

namespace
{

class RenderResourceLoaderImpl : public RenderResourceLoader
{
  public:
    RenderResourceLoaderImpl(ResourceManager& resourceManager, FileSystem& fileSystem,
      render::Renderer& renderer, Logger& logger);

    ResourceHandle loadTextureAsync(const fs::path& path) override;
    ResourceHandle loadNormalMapAsync(const fs::path& path) override;
    ResourceHandle loadCubeMapAsync(const std::array<fs::path, 6>& paths) override;
    MaterialHandle loadMaterialAsync(render::MaterialPtr material) override;
    MeshHandle loadMeshAsync(render::MeshPtr mesh) override;

    bool hasMaterial(const std::string& name) const override;
    bool hasTexture(const std::filesystem::path& path) const override;

    ResourceHandle getMaterialHandle(const std::string& name) const override;
    ResourceHandle getTextureHandle(const std::filesystem::path& path) const override;

    ~RenderResourceLoaderImpl() override;

  private:
    Logger& m_logger;
    ResourceManager& m_resourceManager;
    FileSystem& m_fileSystem;
    render::Renderer& m_renderer;
    std::unordered_map<std::string, ResourceId> m_materials;
    std::unordered_map<std::filesystem::path, ResourceId> m_textures;
};

RenderResourceLoaderImpl::RenderResourceLoaderImpl(ResourceManager& resourceManager,
  FileSystem& fileSystem, render::Renderer& renderer, Logger& logger)
  : m_logger(logger)
  , m_resourceManager(resourceManager)
  , m_fileSystem(fileSystem)
  , m_renderer(renderer)
{}

bool RenderResourceLoaderImpl::hasMaterial(const std::string& name) const
{
  return m_materials.contains(name);
}

bool RenderResourceLoaderImpl::hasTexture(const std::filesystem::path& path) const
{
  return m_textures.contains(path);
}

ResourceHandle RenderResourceLoaderImpl::getMaterialHandle(const std::string& name) const
{
  auto i = m_materials.find(name);
  ASSERT(i != m_materials.end(), "No material with name '" << name << "' loaded");
  return m_resourceManager.getHandle(i->second);
}

ResourceHandle RenderResourceLoaderImpl::getTextureHandle(const std::filesystem::path& path) const
{
  auto i = m_textures.find(path);
  ASSERT(i != m_textures.end(), "No texture with path '" << path << "' loaded");
  return m_resourceManager.getHandle(i->second);
}

ResourceHandle RenderResourceLoaderImpl::loadTextureAsync(const fs::path& path)
{
  DBG_TRACE(m_logger);

  return m_resourceManager.loadResource([this, path](ResourceId id) {
    DBG_TRACE(m_logger);

    auto data = m_fileSystem.readAppDataFile(path);
    auto texture = render::loadRgbaTexture(data);
    m_renderer.addTexture(id, std::move(texture));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeTexture(id);
      }
    };
  });
}

ResourceHandle RenderResourceLoaderImpl::loadNormalMapAsync(const fs::path& path)
{
  DBG_TRACE(m_logger);

  return m_resourceManager.loadResource([this, path](ResourceId id) {
    DBG_TRACE(m_logger);

    auto data = m_fileSystem.readAppDataFile(path);
    auto texture = render::loadRgbaTexture(data);
    m_renderer.addNormalMap(id, std::move(texture));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeTexture(id); // There's no removeNormalMap
      }
    };
  });
}

ResourceHandle RenderResourceLoaderImpl::loadCubeMapAsync(
  const std::array<fs::path, 6>& paths)
{
  DBG_TRACE(m_logger);

  return m_resourceManager.loadResource([this, paths](ResourceId id) {
    DBG_TRACE(m_logger);

    std::array<render::TexturePtr, 6> textures;

    for (size_t i = 0; i < 6; ++i) {
      auto data = m_fileSystem.readAppDataFile(paths[i]);
      textures[i] = render::loadRgbaTexture(data);
    }

    m_renderer.addCubeMap(id, std::move(textures));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeCubeMap(id);
      }
    };
  });
}

MaterialHandle RenderResourceLoaderImpl::loadMaterialAsync(MaterialPtr material)
{
  DBG_TRACE(m_logger);

  auto features = material->featureSet;

  auto loader = [this, m = std::move(material)](ResourceId id) mutable {
    DBG_TRACE(m_logger);

    m_renderer.addMaterial(id, std::move(m));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeMaterial(id);
      }
    };
  };

  return MaterialHandle{
    .resource = m_resourceManager.loadResource(std::move(loader)),
    .features = features
  };
}

MeshHandle RenderResourceLoaderImpl::loadMeshAsync(MeshPtr mesh)
{
  DBG_TRACE(m_logger);

  auto features = mesh->featureSet;

  auto loader = [this, m = std::move(mesh)](ResourceId id) mutable {
    DBG_TRACE(m_logger);

    m_renderer.addMesh(id, std::move(m));

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        DBG_TRACE(m_logger);
        m_renderer.removeMesh(id);
      }
    };
  };

  return MeshHandle{
    .resource = m_resourceManager.loadResource(std::move(loader)),
    .features = features
  };
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
