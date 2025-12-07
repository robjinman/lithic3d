#include "lithic3d/resource_manager.hpp"
#include "lithic3d/thread.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/trace.hpp"
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
    Thread& thread() override;
    void deactivate() override;

    ~ResourceManagerImpl() override;

  private:
    std::atomic<bool> m_deactivated = false;
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

void ResourceManagerImpl::deactivate()
{
  m_deactivated = true;
}

void ResourceManagerImpl::waitAll()
{
  m_thread.waitAll();
}

Thread& ResourceManagerImpl::thread()
{
  return m_thread;
}

// Main thread or worker thread
void ResourceManagerImpl::unload(ResourceId id)
{
  if (m_deactivated.load()) {
    return;
  }

  m_logger.debug(STR("Unload requested for resource " << id));

  auto unloadResource = [this, id]() {
    m_logger.info(STR("Unloading resource " << id));

    auto i = m_resources.find(id);
    if (i != m_resources.end()) {   // Might not be found if loader function threw exception
      try {
        i->second.unloader(id);
      }
      catch (const std::exception& ex) {
        m_logger.error(STR("Error unloading resource " << id << ": " << ex.what()));
      }
      catch (...) {
        m_logger.error(STR("Unknown error unloading resource " << id));
      }

      // i might be invalid after calling unloader, so use id instead
      m_resources.erase(id);
    }
  };

  // We're already on the worker thread
  if (std::this_thread::get_id() == m_thread.id()) {
    unloadResource();
    return;
  }

  m_thread.run<void>(std::move(unloadResource));
}

// Main thread or worker thread
ResourceHandle ResourceManagerImpl::loadResource(ResourceLoader&& loader)
{
  auto id = m_nextResourceId++;

  m_logger.debug(STR("Load requested for resource " << id));

  auto loadResource = [this, id, loader = std::move(loader)]() mutable {
    m_logger.info(STR("Loading resource " << id));

    try {
      m_resources.insert({ id, loader(id) });
    }
    catch (const std::exception& ex) {
      m_logger.error(STR("Error loading resource " << id << ": " << ex.what()));
    }
    catch (...) {
      m_logger.error(STR("Unknown error loading resource " << id));
    }
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
  DBG_TRACE(m_logger);

  waitAll();
}

} // namespace

ResourceManagerPtr createResourceManager(Logger& logger)
{
  return std::make_unique<ResourceManagerImpl>(logger);
}

} // namespace lithic3d
