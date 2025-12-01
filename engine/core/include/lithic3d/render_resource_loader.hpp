#pragma once

#include "renderer.hpp"
#include <filesystem>

namespace lithic3d
{

class RenderResourceLoader
{
  public:
    virtual ResourceHandle loadTextureAsync(const std::filesystem::path& path) = 0;
    virtual ResourceHandle loadMaterialAsync(render::MaterialPtr material) = 0;
    virtual ResourceHandle loadMeshAsync(render::MeshPtr mesh) = 0;

    virtual ~RenderResourceLoader() = default;
};

using RenderResourceLoaderPtr = std::unique_ptr<RenderResourceLoader>;

RenderResourceLoaderPtr createRenderResourceLoader(ResourceManager& resourceManager,
  FileSystem& fileSystem, render::Renderer& renderer);

} // namespace lithic3d
