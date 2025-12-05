#pragma once

#include "renderables.hpp"
#include <filesystem>
#include <map>

namespace lithic3d
{

struct Skin
{
  std::vector<size_t> joints; // Indices into skeleton joints
  std::vector<Mat4x4f> inverseBindMatrices;
};

using SkinPtr = std::unique_ptr<Skin>;

struct AnimationChannel
{
  size_t jointIndex;  // Index into skin
  std::vector<float> timestamps;
  std::vector<Transform> transforms;
};

struct Animation
{
  std::string name;
  // TODO: Duration
  std::vector<AnimationChannel> channels;
};

using AnimationPtr = std::unique_ptr<Animation>;

struct Joint
{
  Transform transform;
  std::vector<size_t> children;
};

struct Skeleton
{
  size_t rootNodeIndex = 0;
  std::vector<Joint> joints;
};

using SkeletonPtr = std::unique_ptr<Skeleton>;

struct AnimationSet
{
  SkeletonPtr skeleton;
  std::map<std::string, AnimationPtr> animations;
};

using AnimationSetPtr = std::unique_ptr<AnimationSet>;

struct Submodel
{
  render::MeshHandle mesh;
  render::MaterialHandle material;
  SkinPtr skin;

  // TODO: Make these private?
  bool jointTransformsDirty = false;
  std::vector<Mat4x4f> jointTransforms;
};

using SubmodelPtr = std::unique_ptr<Submodel>;

struct Model
{
  AnimationSetPtr animations; // TODO: Share between models
  std::vector<SubmodelPtr> submodels;
};

using ModelPtr = std::unique_ptr<Model>;

class ModelLoader
{
  public:
    virtual std::future<Model> loadModelAsync(const std::filesystem::path& path) = 0;

    virtual ~ModelLoader() {}
};

using ModelLoaderPtr = std::unique_ptr<ModelLoader>;

class FileSystem;
class RenderResourceLoader;
class Ecs;

ModelLoaderPtr createModelLoader(RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, const FileSystem& fileSystem, Logger& logger);

} // namespace lithic3d
