#pragma once

#include <vector>
#include <future>
#include <memory>
#include <map> // TODO

namespace lithic3d
{

using ResourceId = int32_t;

constexpr ResourceId NULL_RESOURCE_ID = -1;

using ResourceUnloader = std::function<void(ResourceId)>;

struct Resource
{
  ResourceUnloader unloader;
  std::vector<ResourceId> dependencies;
};

using ResourceLoader = std::function<Resource(ResourceId)>;

class ResourceManager;

class ResourceHandle
{
  friend class ResourceManager;

  public:
    ResourceHandle(const ResourceHandle&) =  delete;

    ResourceHandle(ResourceHandle&& mv)
      : m_resourceManager(mv.m_resourceManager)
      , m_idFuture(std::move(mv.m_idFuture))
      , m_id(mv.m_id)
    {
      mv.m_nullified = true;
    }

    ResourceId id()
    {
      if (m_idFuture.valid()) {
        m_id = m_idFuture.get();
      }
      return m_id;
    }

    ~ResourceHandle();

  private:
    ResourceManager& m_resourceManager;
    std::future<ResourceId> m_idFuture;
    ResourceId m_id = NULL_RESOURCE_ID;
    bool m_nullified = false;

    ResourceHandle(ResourceManager& resourceManager, std::future<ResourceId>&& idFuture)
      : m_resourceManager(resourceManager)
      , m_idFuture(std::move(idFuture))
    {}
};

class ResourceManager
{
  friend class ResourceHandle;

  public:
    virtual ResourceHandle loadResource(ResourceLoader&& loader) = 0;
    virtual std::future<void> unloadResource(ResourceId id) = 0;

    virtual ~ResourceManager() = default;

  protected:
    ResourceHandle makeHandle(std::future<ResourceId>&& f)
    {
      return ResourceHandle{*this, std::move(f)};
    }

  private:
    virtual void decrementReferenceCount(ResourceId id) = 0;
};

using ResourceManagerPtr = std::unique_ptr<ResourceManager>;

inline ResourceHandle::~ResourceHandle()
{
  if (!m_nullified) {
    m_resourceManager.decrementReferenceCount(id());
  }
}

class Logger;

ResourceManagerPtr createResourceManager(Logger& logger);
/*
class SubthingFactory
{
  public:
    SubthingFactory(ResourceManager& resourceManager)
      : m_resourceManager(resourceManager)
    {}

    std::future<ResourceId> createSubthingAsync(int bar)
    {
      // We might already be on the resource manager thread. The resource manager must check which
      // thread we're on and execute the lambda immediately if we're on the resource manager
      // thread. If it queues it, we'll end up in deadlock.

      return m_resourceManager.loadResource([this, bar](ResourceId id) {
        return createSubthing(id, bar);
      });
    }

  private:
    ResourceManager& m_resourceManager;

    Resource createSubthing(ResourceId id, int bar)
    {
      // TODO

      return Resource{
        .unloader = [this](ResourceId id) { deleteSubthing(id); },
        .dependencies = {}
      };
    }

    void deleteSubthing(ResourceId id)
    {
    }
};

struct Thing
{
  std::string foo;
  ResourceId subthing = NULL_RESOURCE_ID;
};

class ThingFactory
{
  public:
    ThingFactory(ResourceManager& resourceManager, SubthingFactory subthingFactory)
      : m_resourceManager(resourceManager)
      , m_subthingFactory(subthingFactory)
    {}

    std::future<ResourceId> createThingAsync(const std::string& foo, int bar)
    {
      return m_resourceManager.loadResource([this, foo, bar](ResourceId id) {
        return createThing(id, foo, bar);
      });
    }

  private:
    ResourceManager& m_resourceManager;
    SubthingFactory& m_subthingFactory;
    std::map<ResourceId, std::unique_ptr<Thing>> m_things;

    Resource createThing(ResourceId id, const std::string& foo, int bar)
    {
      // We are already in the resource manager thread at this point

      auto subthingId = m_subthingFactory.createSubthingAsync(bar).get();

      return Resource{
        .unloader = [this](ResourceId id) { deleteThing(id); },
        .dependencies = { subthingId }
      };
    }

    void deleteThing(ResourceId id)
    {
    }
};

void foo(ResourceManager& resourceManager)
{
  SubthingFactory subthingFactory{resourceManager};
  ThingFactory thingFactory{resourceManager, subthingFactory};

  std::string foo = "Hello";
  int bar = 123;
  auto thingFuture = thingFactory.createThingAsync(foo, bar);
}
*/
} // namespace lithic3d
