#pragma once

#include "resource_manager.hpp"
#include <map>

namespace lithic3d
{

struct Submodel
{
  render::MeshPtr mesh;
  render::MaterialPtr material;
  SkinPtr skin; // TODO: Share skins between submodels
};

using SubmodelPtr = std::unique_ptr<Submodel>;

struct Model
{
  std::vector<SubmodelPtr> submodels;
  AnimationSetPtr animations;
};

using ModelPtr = std::unique_ptr<Model>;

class ModelLoader
{
  public:
    virtual ResourceId loadModel(const std::string& filePath) const = 0;
    virtual DModelPtr createRenderComponent(ResourceId model, bool isInstanced) = 0;

    virtual ~ModelLoader() {}
};

using ModelLoaderPtr = std::unique_ptr<ModelLoader>;

class FileSystem;
class Renderer;
class Logger;

ModelLoaderPtr createModelLoader(ResourceManager& resourceManager, Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger);

} // namespace lithic3d
