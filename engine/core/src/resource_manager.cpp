#include "lithic3d/resource_manager.hpp"
#include "lithic3d/thread.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include <unordered_map>
#include <mutex>

namespace lithic3d
{
namespace
{

struct ManagedResource
{
  Resource data;
  uint32_t referenceCount = 0;
  bool loaded = false;
};

class ResourceManagerImpl : public ResourceManager
{
  public:
    ResourceManagerImpl(Logger& logger);

    ResourceId nextResourceId() override;
    void addResource(Resource&& resource) override;
    std::future<void> loadResources(const std::vector<ResourceId>& resources) override;
    std::future<void> unloadResources(const std::vector<ResourceId>& resources) override;

  private:
    Logger& m_logger;
    ResourceId m_nextResourceId = NULL_RESOURCE_ID + 1;
    std::unordered_map<ResourceId, ManagedResource> m_resources;
    std::recursive_mutex m_mutex;
    Thread m_thread;

    void loadResource(ResourceId id, bool incrementRefCount);
    void unloadResource(ResourceId id);
};

ResourceManagerImpl::ResourceManagerImpl(Logger& logger)
  : m_logger(logger)
{
}

ResourceId ResourceManagerImpl::nextResourceId()
{
  return m_nextResourceId++;
}

void ResourceManagerImpl::addResource(Resource&& resource)
{
  std::scoped_lock lock{m_mutex};

  m_resources.insert({ resource.id, ManagedResource{
    .data = std::move(resource),
    .referenceCount = 0
  }});
}

// Worker thread
void ResourceManagerImpl::loadResource(ResourceId id, bool incrementRefCount)
{
  std::scoped_lock lock{m_mutex};

  auto i = m_resources.find(id);
  ASSERT(i != m_resources.end(), "No resource with id " << id);
  auto& resource = i->second;

  if (resource.loaded) {
    if (incrementRefCount) {
      ++resource.referenceCount;
    }
    return; // Already loaded
  }

  assert(resource.referenceCount == 0);
  if (incrementRefCount) {
    ++resource.referenceCount;
  }

  for (auto depId : resource.data.dependencies) {
    loadResource(depId, true);
  }

  m_logger.debug(STR("Loading resource " << id));
  try {
    resource.data.loader();
    resource.loaded = true;
  }
  catch (const Exception& ex) {
    m_logger.error(ex.what());
  }
}

// Worker thread
void ResourceManagerImpl::unloadResource(ResourceId id)
{
  std::scoped_lock lock{m_mutex};

  auto i = m_resources.find(id);
  ASSERT(i != m_resources.end(), "No resource with id " << id);
  auto& resource = i->second;

  assert(resource.data.id == id);

  if (!resource.loaded) {
    assert(resource.referenceCount == 0);
    return; // Already unloaded
  }

  if (resource.referenceCount > 0) {
    // Can't unload. Something else depends on it
    return;
  }

  m_logger.debug(STR("Unloading resource " << id));
  try {
    resource.data.unloader();
    resource.loaded = false;
  }
  catch (const Exception& ex) {
    m_logger.error(ex.what());
  }

  for (auto depId : resource.data.dependencies) {
    auto& dep = m_resources.at(depId);
    assert(dep.referenceCount > 0);
    assert(dep.loaded);

    --dep.referenceCount;

    if (dep.referenceCount == 0) {
      unloadResource(depId);
    }
  }
}

std::future<void> ResourceManagerImpl::loadResources(const std::vector<ResourceId>& resources)
{
  return m_thread.run<void>([this, resources]() {
    for (auto& resourceId : resources) {
      loadResource(resourceId, false);
    }
  });
}

std::future<void> ResourceManagerImpl::unloadResources(const std::vector<ResourceId>& resources)
{
  return m_thread.run<void>([this, resources]() {
    for (auto& resourceId : resources) {
      unloadResource(resourceId);
    }
  });
}

} // namespace

ResourceManagerPtr createResourceManager(Logger& logger)
{
  return std::make_unique<ResourceManagerImpl>(logger);
}

} // namespace lithic3d
