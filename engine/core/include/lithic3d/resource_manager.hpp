#pragma once

#include <vector>
#include <future>

namespace lithic3d
{

using ResourceId = int32_t;

constexpr ResourceId NULL_RESOURCE_ID = -1;

using ResourceLoader = std::function<void()>;
using ResourceUnloader = std::function<void()>;

struct Resource
{
  ResourceId id = NULL_RESOURCE_ID;
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
