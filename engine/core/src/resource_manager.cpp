#include "lithic3d/resource_manager.hpp"
#include "lithic3d/thread.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include <unordered_map>

namespace lithic3d
{
namespace
{

struct ResourceData
{
  Resource data;
  uint32_t referenceCount = 0;
  ResourceHandlePtr handle = nullptr;
};

class ResourceManagerImpl : public ResourceManager
{
  public:
    ResourceManagerImpl(Logger& logger);

    ResourceId nextResourceId() override;
    void addResource(const Resource& resource) override;
    std::future<void> loadResources(const std::vector<ResourceId>& resources) override;
    std::future<void> unloadResources(const std::vector<ResourceId>& resources) override;
    const ResourceHandle& getHandle(ResourceId resourceId) const override;

  private:
    Logger& m_logger;
    ResourceId m_nextResourceId = NULL_RESOURCE_ID + 1;
    std::unordered_map<ResourceId, ResourceData> m_resources;
    Thread m_thread;

    // TODO: Mutex

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

const ResourceHandle& ResourceManagerImpl::getHandle(ResourceId resourceId) const
{
  auto i = m_resources.find(resourceId);
  if (i == m_resources.end()) {
    EXCEPTION("No such resource with id " << resourceId);
  }
  if (i->second.handle == nullptr) {
    EXCEPTION("Resource with id " << resourceId << " is not loaded");
  }

  return *i->second.handle;
}

void ResourceManagerImpl::addResource(const Resource& resource)
{
  m_resources.insert({ resource.id, ResourceData{
    .data = resource,
    .referenceCount = 0,
    .handle = nullptr
  }});
}

// Worker thread
void ResourceManagerImpl::loadResource(ResourceId id, bool incrementRefCount)
{
  auto& resource = m_resources.at(id);
  if (resource.handle != nullptr) {
    if (incrementRefCount) {
      ++resource.referenceCount;
    }
    return; // Already loaded
  }

  assert(resource.referenceCount == 0);
  if (incrementRefCount) {
    ++resource.referenceCount;
  }

  ResourceHandleList deps;
  for (auto depId : resource.data.dependencies) {
    loadResource(depId, true);
    auto& dep = m_resources.at(depId);

    deps.push_back(*dep.handle);
  }

  m_logger.debug(STR("Loading resource " << id));
  try {
    resource.handle = resource.data.loader(deps);
  }
  catch (const Exception& ex) {
    m_logger.error(ex.what());
  }
}

// Worker thread
void ResourceManagerImpl::unloadResource(ResourceId id)
{
  auto& resource = m_resources.at(id);
  if (resource.handle == nullptr) {
    assert(resource.referenceCount == 0);
    return; // Already unloaded
  }

  if (resource.referenceCount > 0) {
    // Can't unload. Something else depends on it
    return;
  }

  m_logger.debug(STR("Unloading resource " << id));
  try {
    resource.data.unloader(*resource.handle);
  }
  catch (const Exception& ex) {
    m_logger.error(ex.what());
  }
  resource.handle.reset();

  for (auto depId : resource.data.dependencies) {
    auto& dep = m_resources.at(depId);
    assert(dep.referenceCount > 0);
    assert(dep.handle != nullptr);

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
