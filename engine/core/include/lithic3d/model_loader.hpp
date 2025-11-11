#pragma once

#include "sys_render_3d.hpp"
#include <map>

namespace lithic3d
{

struct SubmodelData
{
  render::MeshPtr mesh;
  render::MaterialPtr material;
  SkinPtr skin; // TODO: Share skins between submodels
};

using SubmodelDataPtr = std::unique_ptr<SubmodelData>;

struct ModelData
{
  std::vector<SubmodelDataPtr> submodels;
  AnimationSetPtr animations;
};

using ModelDataPtr = std::unique_ptr<ModelData>;

class ModelLoader
{
  public:
    virtual ModelDataPtr loadModelData(const std::string& filePath) const = 0;
    //virtual CRenderModelPtr createRenderComponent(ModelDataPtr modelData, bool isInstanced) = 0;

    virtual ~ModelLoader() {}
};

using ModelLoaderPtr = std::unique_ptr<ModelLoader>;

class FileSystem;
class Logger;

ModelLoaderPtr createModelLoader(SysRender3d& sysRender, const FileSystem& fileSystem,
  Logger& logger);

} // namespace lithic3d
