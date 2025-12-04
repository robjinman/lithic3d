#pragma once

#include "renderer.hpp"
#include <filesystem>

namespace lithic3d
{

class RenderResourceLoader
{
  public:
    virtual ResourceHandle loadTextureAsync(const std::filesystem::path& path) = 0;
    virtual ResourceHandle loadNormalMapAsync(const std::filesystem::path& path) = 0;
    virtual ResourceHandle loadCubeMapAsync(const std::array<std::filesystem::path, 6>& path) = 0;
    virtual render::MaterialHandle loadMaterialAsync(render::MaterialPtr material) = 0;
    virtual render::MeshHandle loadMeshAsync(render::MeshPtr mesh) = 0;

    virtual ~RenderResourceLoader() = default;
};

using RenderResourceLoaderPtr = std::unique_ptr<RenderResourceLoader>;

RenderResourceLoaderPtr createRenderResourceLoader(ResourceManager& resourceManager,
  FileSystem& fileSystem, render::Renderer& renderer, Logger& logger);

} // namespace lithic3d
