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

    ResourceHandle loadResource(ResourceLoader&& loader) override;
    std::future<void> unloadResource(ResourceId id) override;

  private:
    Logger& m_logger;
    ResourceId m_nextResourceId = NULL_RESOURCE_ID + 1;
    std::unordered_map<ResourceId, ManagedResource> m_resources;
    Thread m_thread;

    void decrementReferenceCount(ResourceId id) override;

    void manageResource(ResourceId id, Resource&& resource);
    void doUnload(ResourceId id);
    void doDecrementRefCount(ResourceId id);
};

ResourceManagerImpl::ResourceManagerImpl(Logger& logger)
  : m_logger(logger)
{
}

// Main thread
void ResourceManagerImpl::decrementReferenceCount(ResourceId id)
{
  // TODO: Support this?
  ASSERT(std::this_thread::get_id() != m_thread.id(),
    "Cannot call decrementReferenceCount from resource manager thread");

  m_thread.run<void>([this, id]() {
    doDecrementRefCount(id);
  }); // Don't wait
}

// Worker thread
void ResourceManagerImpl::doDecrementRefCount(ResourceId id)
{
  auto& dep = m_resources.at(id);
  assert(dep.referenceCount > 0);

  --dep.referenceCount;

  if (dep.referenceCount == 0) {
    doUnload(id);
  }
}

// Worker thread
void ResourceManagerImpl::manageResource(ResourceId id, Resource&& resource)
{
  for (auto dep : resource.dependencies) {
    ++m_resources.at(dep).referenceCount;
  }

  m_resources.insert({id, ManagedResource{
    .data = std::move(resource),
    .referenceCount = 1
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
    doDecrementRefCount(depId);
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
ResourceHandle ResourceManagerImpl::loadResource(ResourceLoader&& loader)
{
  if (std::this_thread::get_id() == m_thread.id()) {
    auto id = m_nextResourceId++;
    manageResource(id, loader(id));

    std::promise<ResourceId> promise;
    promise.set_value(id);

    return makeHandle(promise.get_future());
  }

  return makeHandle(m_thread.run<ResourceId>([this, loader = std::move(loader)]() {
    auto id = m_nextResourceId++;
    manageResource(id, loader(id));
    return id;
  }));
}

} // namespace

ResourceManagerPtr createResourceManager(Logger& logger)
{
  return std::make_unique<ResourceManagerImpl>(logger);
}

} // namespace lithic3d
