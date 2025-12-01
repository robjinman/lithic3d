#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/file_system.hpp"

namespace lithic3d
{

using render::MeshPtr;
using render::MaterialPtr;

namespace
{

class RenderResourceLoaderImpl : public RenderResourceLoader, public ResourceProvider
{
  public:
    RenderResourceLoaderImpl(ResourceManager& resourceManager, FileSystem& fileSystem,
      render::Renderer& renderer);

    ResourceHandle loadTextureAsync(const std::filesystem::path& path) override;
    ResourceHandle loadMaterialAsync(render::MaterialPtr material) override;
    ResourceHandle loadMeshAsync(render::MeshPtr mesh) override;

  private:
    ResourceManager& m_resourceManager;
    FileSystem& m_fileSystem;
    render::Renderer& m_renderer;
};

RenderResourceLoaderImpl::RenderResourceLoaderImpl(ResourceManager& resourceManager,
  FileSystem& fileSystem, render::Renderer& renderer)
  : m_resourceManager(resourceManager)
  , m_fileSystem(fileSystem)
  , m_renderer(renderer)
{}

ResourceHandle RenderResourceLoaderImpl::loadTextureAsync(const std::filesystem::path& path)
{
  return m_resourceManager.loadResource([this, path](ResourceId id) {
    auto data = m_fileSystem.readAppDataFile(path);
    auto texture = render::loadTexture(data);
    m_renderer.addTexture(id, std::move(texture));

    return ManagedResource{
      .provider = weak_from_this(),
      .unloader = [this](ResourceId id) {
        m_renderer.removeTexture(id);
      }
    };
  });
}

ResourceHandle RenderResourceLoaderImpl::loadMaterialAsync(MaterialPtr material)
{
  return m_resourceManager.loadResource([this, m = std::move(material)](ResourceId id) mutable {
    m_renderer.addMaterial(id, std::move(m));

    return ManagedResource{
      .provider = weak_from_this(),
      .unloader = [this](ResourceId id) {
        m_renderer.removeMaterial(id);
      }
    };
  });
}

ResourceHandle RenderResourceLoaderImpl::loadMeshAsync(MeshPtr mesh)
{
  return m_resourceManager.loadResource([this, m = std::move(mesh)](ResourceId id) mutable {
    m_renderer.addMesh(id, std::move(m));

    return ManagedResource{
      .provider = weak_from_this(),
      .unloader = [this](ResourceId id) {
        m_renderer.removeMesh(id);
      }
    };
  });
}

} // namespace

RenderResourceLoaderPtr createRenderResourceLoader(ResourceManager& resourceManager,
  FileSystem& fileSystem, render::Renderer& renderer)
{
  return std::make_unique<RenderResourceLoaderImpl>(resourceManager, fileSystem, renderer);
}

} // lithic3d
