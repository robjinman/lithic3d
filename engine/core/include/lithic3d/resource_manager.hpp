#pragma once

#include <future>
#include <memory>

namespace lithic3d
{

using ResourceId = int32_t;

constexpr ResourceId NULL_RESOURCE_ID = -1;

using ResourceUnloader = std::function<void(ResourceId)>;

struct ResourceProvider : protected std::enable_shared_from_this<ResourceProvider>
{
  virtual ~ResourceProvider() = default;
};

struct ManagedResource
{
  std::weak_ptr<ResourceProvider> provider;
  ResourceUnloader unloader;
};

using ResourceLoader = std::function<ManagedResource(ResourceId)>;

class ResourceManager;

class Resource
{
  public:
    Resource(ResourceId id, ResourceManager& resourceManager, std::future<void>&& future)
      : m_id(id)
      , m_resourceManager(resourceManager)
      , m_future(std::move(future))
    {}

    void wait() const
    {
      m_future.wait();
    }

    ~Resource();

  private:
    ResourceId m_id;
    ResourceManager& m_resourceManager;
    ResourceUnloader m_unloader;
    std::future<void> m_future;
};

class [[nodiscard]] ResourceHandle
{
  public:
    ResourceHandle()
      : m_id(NULL_RESOURCE_ID)
    {}

    ResourceHandle(ResourceId id, const std::shared_ptr<Resource>& pointer)
      : m_id(id)
      , m_resource(pointer)
    {}

    ResourceHandle(const ResourceHandle& cpy)
      : m_id(cpy.m_id)
      , m_resource(cpy.m_resource)
    {}

    ResourceId id() const
    {
      return m_id;
    }

    ResourceHandle& operator=(const ResourceHandle& rhs)
    {
      m_id = rhs.m_id;
      m_resource = rhs.m_resource;
      return *this;
    }

    bool operator==(const ResourceHandle& rhs) const
    {
      return m_id == rhs.m_id;
    }

    bool operator<(const ResourceHandle& rhs) const
    {
      return m_id < rhs.m_id;
    }

    void wait() const
    {
      if (m_resource != nullptr) {
        m_resource->wait();
      }
    }

    void reset()
    {
      m_id = NULL_RESOURCE_ID;
      m_resource.reset();
    }

    operator bool() const
    {
      return !(m_id == NULL_RESOURCE_ID && m_resource == nullptr);
    }

  private:
    ResourceId m_id = NULL_RESOURCE_ID;
    std::shared_ptr<Resource> m_resource;
};

class ResourceManager
{
  friend class Resource;

  public:
    virtual ResourceHandle loadResource(ResourceLoader&& loader) = 0;
    virtual void waitAll() = 0;

    virtual ~ResourceManager() = default;

  private:
    virtual void unload(ResourceId id) = 0;
};

using ResourceManagerPtr = std::unique_ptr<ResourceManager>;

inline Resource::~Resource()
{
  m_resourceManager.unload(m_id);
}

class Logger;

ResourceManagerPtr createResourceManager(Logger& logger);

} // namespace lithic3d
