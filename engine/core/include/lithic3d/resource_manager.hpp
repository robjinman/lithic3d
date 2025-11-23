#pragma once

#include "work_queue.hpp"
#include <vector>

namespace lithic3d
{

using ResourceId = size_t;
using ResourceGroupId = size_t;

using ResourceLoader = std::function<void()>;
using ResourceUnloader = std::function<void()>;

struct Resource
{
  // A resource with disposability 0 is never unloaded.
  // More disposable resources get unloaded first when memory is full.
  uint32_t disposability = 0;
  ResourceLoader loader;
  ResourceUnloader unloader;
  std::vector<ResourceId> dependencies;
};

using ResourcePtr = std::unique_ptr<Resource>;

class ResourceManager
{
  public:
    virtual ResourceGroupId newResourceGroup() = 0;
    virtual void addResource(ResourceGroupId groupId, ResourcePtr resource) = 0;
    virtual WorkItemResult loadResourceGroup(ResourceGroupId id) = 0;
    virtual bool resourceIsLoaded() const = 0;

    // Marks resources within the group for unload. They'll be unloaded when their reference count
    // reaches zero.
    virtual void unloadResourceGroup(ResourceGroupId id) = 0;

    virtual ~ResourceManager() = default;
};

using ResourceManagerPtr = std::unique_ptr<ResourceManager>;

} // namespace lithic3d
