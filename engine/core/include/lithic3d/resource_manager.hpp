// Almost all resources in Lithic3D are managed resources: Sounds, Meshes, Textures, Models,
// Entities, etc. Some of these resources are made up of other resources and some resources are
// shared among multiple resources.
//
// The ResourceManager class keeps track of which resources depend on other resources and uses
// reference counting to ensure that resources are only unloaded when no longer needed.
//
// Any class that provides managed resources, whether that's a factory or a subsystem, must
// inherit from ResourceProvider and override its createResource method. Within the createResource
// method the derived class should register the resource with the ResourceManager via the
// addResource method providing a Resource instance describing the resource.
//
// The resource manager when it needs to load or unload the resource will call the loader or
// unloader functions provided by the Resource instance. These functions may be called at any time
// from the resource manager's own thread and so must be thread-safe.
//
// See the unit tests for examples.

#pragma once

#include <vector>
#include <future>

namespace lithic3d
{

using ResourceId = int32_t;

constexpr ResourceId NULL_RESOURCE_ID = -1;

class ResourceData
{
  public:
    virtual ~ResourceData() = default;
};

using ResourceDataPtr = std::unique_ptr<ResourceData>;

// Helpful wrapper that allows you to aggregate-initialise T
template<typename T>
struct ResourceDataWrapper : public ResourceData
{
  ResourceDataWrapper(const T& data)
    : data(data) {}

  T data;
};

class ResourceProvider
{
  public:
    virtual ResourceId createResource(ResourceDataPtr data) = 0;

    virtual ~ResourceProvider() = default;
};

class Resource;

using ResourceLoader = std::function<void(const Resource& resource)>;
using ResourceUnloader = std::function<void(ResourceId id)>;

struct Resource
{
  ResourceId id = NULL_RESOURCE_ID;
  ResourceDataPtr data;
  ResourceLoader loader;
  ResourceUnloader unloader;
  std::vector<ResourceId> dependencies;
};

class ResourceManager
{
  public:
    virtual ResourceId nextResourceId() = 0;
    virtual void addResource(Resource&& resource) = 0;
    virtual std::future<void> loadResources(const std::vector<ResourceId>& resources) = 0;
    virtual std::future<void> unloadResources(const std::vector<ResourceId>& resources) = 0;

    virtual ~ResourceManager() = default;
};

using ResourceManagerPtr = std::unique_ptr<ResourceManager>;

class Logger;

ResourceManagerPtr createResourceManager(Logger& logger);

} // namespace lithic3d
