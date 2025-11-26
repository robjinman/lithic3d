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

struct ManagedResource
{
  Resource data;
  uint32_t referenceCount = 0;
};

class ResourceManagerImpl : public ResourceManager
{
  public:
    ResourceManagerImpl(Logger& logger);

    std::future<ResourceId> loadResource(ResourceLoader&& loader) override;
    std::future<void> unloadResource(ResourceId id) override;

  private:
    Logger& m_logger;
    ResourceId m_nextResourceId = NULL_RESOURCE_ID + 1;
    std::unordered_map<ResourceId, ManagedResource> m_resources;
    Thread m_thread;

    void manageResource(ResourceId id, Resource&& resource);
    void doUnload(ResourceId id);
};

ResourceManagerImpl::ResourceManagerImpl(Logger& logger)
  : m_logger(logger)
{
}

// Worker thread
void ResourceManagerImpl::manageResource(ResourceId id, Resource&& resource)
{
  for (auto dep : resource.dependencies) {
    ++m_resources.at(dep).referenceCount;
  }

  m_resources.insert({id, ManagedResource{
    .data = std::move(resource),
    .referenceCount = 0
  }});
}

// Worker thread
void ResourceManagerImpl::doUnload(ResourceId id)
{
  auto i = m_resources.find(id);
  ASSERT(i != m_resources.end(), "No resource with id " << id);
  auto& resource = i->second;

  if (resource.referenceCount > 0) {
    // Can't unload. Something else depends on it
    return;
  }

  // Make a copy
  auto dependencies = resource.data.dependencies;

  m_logger.debug(STR("Unloading resource " << id));
  try {
    resource.data.unloader(id);
    m_resources.erase(i);
  }
  catch (const Exception& ex) {
    m_logger.error(ex.what());
  }

  for (auto depId : dependencies) {
    auto& dep = m_resources.at(depId);
    assert(dep.referenceCount > 0);

    --dep.referenceCount;

    if (dep.referenceCount == 0) {
      doUnload(depId);
    }
  }
}

// Main thread
std::future<void> ResourceManagerImpl::unloadResource(ResourceId id)
{
  return m_thread.run<void>([this, id]() {
    doUnload(id);
  });
}

// Main thread or worker thread
std::future<ResourceId> ResourceManagerImpl::loadResource(ResourceLoader&& loader)
{
  if (std::this_thread::get_id() == m_thread.id()) {
    auto id = m_nextResourceId++;
    manageResource(id, loader(id));

    std::promise<ResourceId> promise;
    promise.set_value(id);

    return promise.get_future();
  }

  return m_thread.run<ResourceId>([this, loader = std::move(loader)]() {
    auto id = m_nextResourceId++;
    manageResource(id, loader(id));
    return id;
  });
}

} // namespace

ResourceManagerPtr createResourceManager(Logger& logger)
{
  return std::make_unique<ResourceManagerImpl>(logger);
}

} // namespace lithic3d
