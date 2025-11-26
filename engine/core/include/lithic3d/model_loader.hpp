#pragma once

#include "resource_manager.hpp"
#include <map>

namespace lithic3d
{

class ModelLoader
{
  public:
    virtual ResourceId loadModel(const std::string& filePath) const = 0;
    //virtual DModelPtr createRenderComponent(ResourceId model, bool isInstanced) = 0;

    virtual ~ModelLoader() {}
};

using ModelLoaderPtr = std::unique_ptr<ModelLoader>;

class FileSystem;
namespace render { class Renderer; }
class Logger;

ModelLoaderPtr createModelLoader(ResourceManager& resourceManager, render::Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger);

} // namespace lithic3d
