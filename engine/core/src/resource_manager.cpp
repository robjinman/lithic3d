#include "lithic3d/resource_manager.hpp"
#include "lithic3d/thread.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/exception.hpp"
#include <unordered_map>
#include <atomic>

namespace lithic3d
{
namespace
{

class ResourceManagerImpl : public ResourceManager
{
  public:
    ResourceManagerImpl(Logger& logger);

    ResourceHandle loadResource(ResourceLoader&& loader) override;
    void waitAll() override;

    ~ResourceManagerImpl();

  private:
    Logger& m_logger;
    std::atomic<ResourceId> m_nextResourceId = NULL_RESOURCE_ID + 1;
    Thread m_thread;
    std::unordered_map<ResourceId, ManagedResource> m_resources;

    void unload(ResourceId id) override;
};

ResourceManagerImpl::ResourceManagerImpl(Logger& logger)
  : m_logger(logger)
{
}

void ResourceManagerImpl::waitAll()
{
  m_thread.waitAll();
}

// Main thread or worker thread
void ResourceManagerImpl::unload(ResourceId id)
{
  m_logger.debug(STR("Unload requested for resource " << id));

  auto unloadResource = [this, id]() {
    m_logger.info(STR("Unloading resource " << id));

    auto i = m_resources.find(id);

    {
      auto provider = i->second.provider.lock();
      if (provider) {
        i->second.unloader(id);
      }
    }

    m_resources.erase(i);
  };

  // We're already on the worker thread
  if (std::this_thread::get_id() == m_thread.id()) {
    unloadResource();
    return;
  }

  m_thread.run<void>(unloadResource);
}

// Main thread or worker thread
ResourceHandle ResourceManagerImpl::loadResource(ResourceLoader&& loader)
{
  auto id = m_nextResourceId++;

  auto loadResource = [this, id, loader = std::move(loader)]() {
    m_logger.info(STR("Loading resource " << id));
    m_resources.insert({ id, loader(id)});
  };

  // We're already on the worker thread. Just load the resource synchronously and return a dummy
  // future.
  if (std::this_thread::get_id() == m_thread.id()) {
    loadResource();

    std::promise<void> promise;
    return ResourceHandle(id, std::make_shared<Resource>(id, *this, promise.get_future()));
  }

  auto future = m_thread.run<void>(std::move(loadResource));

  return ResourceHandle(id, std::make_shared<Resource>(id, *this, std::move(future)));
}

ResourceManagerImpl::~ResourceManagerImpl()
{
  waitAll();
}

} // namespace

ResourceManagerPtr createResourceManager(Logger& logger)
{
  return std::make_unique<ResourceManagerImpl>(logger);
}

} // namespace lithic3d
