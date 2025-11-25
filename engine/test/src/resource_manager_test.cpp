#include "mock_logger.hpp"
#include <lithic3d/resource_manager.hpp>
#include <gtest/gtest.h>
#include <vector>
#include <unordered_map>

using testing::NiceMock;
using testing::Return;
using namespace lithic3d;

class ResourceManagerTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

class Delegate
{
  public:
    MOCK_METHOD(void, loadAObject, ());
    MOCK_METHOD(void, unloadAObject, ());

    MOCK_METHOD(void, loadBObject, ());
    MOCK_METHOD(void, unloadBObject, ());

    MOCK_METHOD(void, loadCObject, ());
    MOCK_METHOD(void, unloadCObject, ());

    MOCK_METHOD(void, loadDObject, ());
    MOCK_METHOD(void, unloadDObject, ());
};

// Data needed to construct an AObject
struct AObjectData
{
  std::string foo;
};

using AObjectDataWrapper = ResourceDataWrapper<AObjectData>;

struct AObject
{
  std::string foo;
};

using AObjectPtr = std::unique_ptr<AObject>;

class AObjectFactory : public ResourceProvider
{
  public:
    AObjectFactory(Delegate& delegate, ResourceManager& resourceManager)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
    {}

    ResourceId createResource(ResourceDataPtr data) override
    {
      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .data = std::move(data),
        .loader = [this](const Resource& resource) { loadAObject(resource); },
        .unloader = [this](ResourceId id) { unloadAObject(id); },
        .dependencies = {}
      });

      return id;
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    std::unordered_map<ResourceId, AObjectPtr> m_objects;

    // Called from resource manager thread
    void loadAObject(const Resource& resource)
    {
      auto& data = dynamic_cast<const AObjectDataWrapper&>(*resource.data);
      m_objects.insert({ resource.id, std::make_unique<AObject>(data.data.foo) });

      m_delegate.loadAObject();
    }

    // Called from resource manager thread
    void unloadAObject(ResourceId id)
    {
      m_objects.erase(id);

      m_delegate.unloadAObject();
    }
};

struct BObjectData
{
  std::string foo;
  int bar = 0;
};

using BObjectDataWrapper = ResourceDataWrapper<BObjectData>;

// Demonstrates a resource that depends on another resource, e.g. a material that contains a texture
struct BObject
{
  int bar = 0;
  ResourceId aObject = NULL_RESOURCE_ID;
};

using BObjectPtr = std::unique_ptr<BObject>;

class BObjectFactory : public ResourceProvider
{
  public:
    BObjectFactory(Delegate& delegate, ResourceManager& resourceManager,
      AObjectFactory& aObjectFactory)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
      , m_aObjectFactory(aObjectFactory)
    {}

    ResourceId createResource(ResourceDataPtr pData) override
    {
      auto& data = dynamic_cast<const BObjectDataWrapper&>(*pData);

      auto aObject = std::make_unique<AObjectDataWrapper>(AObjectData{
        .foo = data.data.foo
      });
      auto aObjId = m_aObjectFactory.createResource(std::move(aObject));

      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .data = std::move(pData),
        .loader = [this](const Resource& resource) { loadBObject(resource); },
        .unloader = [this](ResourceId id) { unloadBObject(id); },
        .dependencies = { aObjId }
      });

      return id;
    }

    const BObject& get(ResourceId id) const
    {
      return *m_objects.at(id);
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    AObjectFactory& m_aObjectFactory;
    std::unordered_map<ResourceId, BObjectPtr> m_objects;

    // Called from resource manager thread
    void loadBObject(const Resource& resource)
    {
      auto& data = dynamic_cast<const BObjectDataWrapper&>(*resource.data);
      m_objects.insert({ resource.id, std::make_unique<BObject>(BObject{
        .bar = data.data.bar,
        .aObject = resource.dependencies[0]
      })});

      m_delegate.loadBObject();
    }

    // Called from resource manager thread
    void unloadBObject(ResourceId id)
    {
      m_objects.erase(id);

      m_delegate.unloadBObject();
    }
};

struct CObjectData
{
  std::string foo;
  int bar = 0;
  uint64_t baz = 0;
};

using CObjectDataWrapper = ResourceDataWrapper<CObjectData>;

struct CObject
{
  uint64_t baz = 0;
  ResourceId bObject = NULL_RESOURCE_ID;
};

using CObjectPtr = std::unique_ptr<CObject>;

class CObjectFactory : public ResourceProvider
{
  public:
    CObjectFactory(Delegate& delegate, ResourceManager& resourceManager,
      BObjectFactory& bObjectFactory)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
      , m_bObjectFactory(bObjectFactory)
    {}

    ResourceId createResource(ResourceDataPtr pData) override
    {
      auto& data = dynamic_cast<const CObjectDataWrapper&>(*pData);

      auto bObject = std::make_unique<BObjectDataWrapper>(BObjectData{
        .foo = data.data.foo,
        .bar = data.data.bar
      });
      auto bObjId = m_bObjectFactory.createResource(std::move(bObject));

      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .data = std::move(pData),
        .loader = [this](const Resource& resource) { loadCObject(resource); },
        .unloader = [this](ResourceId id) { unloadCObject(id); },
        .dependencies = { bObjId }
      });

      return id;
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    BObjectFactory& m_bObjectFactory;
    std::unordered_map<ResourceId, CObjectPtr> m_objects;

    // Called from resource manager thread
    void loadCObject(const Resource& resource)
    {
      auto& data = dynamic_cast<const CObjectDataWrapper&>(*resource.data);
      m_objects.insert({ resource.id, std::make_unique<CObject>(CObject{
        .baz = data.data.baz,
        .bObject = resource.dependencies[0]
      })});

      m_delegate.loadCObject();
    }

    // Called from resource manager thread
    void unloadCObject(ResourceId id)
    {
      m_objects.erase(id);

      m_delegate.unloadCObject();
    }
};

struct DObjectData
{
  ResourceId aObject = NULL_RESOURCE_ID;
  char whizz = 0;
};

struct DObject
{
  char whizz = 0;
  ResourceId aObject = NULL_RESOURCE_ID;
};

using DObjectDataWrapper = ResourceDataWrapper<DObjectData>;

using DObjectPtr = std::unique_ptr<DObject>;

class DObjectFactory : public ResourceProvider
{
  public:
    DObjectFactory(Delegate& delegate, ResourceManager& resourceManager,
      AObjectFactory& aObjectFactory)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
      , m_aObjectFactory(aObjectFactory)
    {}

    ResourceId createResource(ResourceDataPtr pData) override
    {
      auto& data = dynamic_cast<const DObjectDataWrapper&>(*pData);

      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .data = std::move(pData),
        .loader = [this](const Resource& resource) { loadDObject(resource); },
        .unloader = [this](ResourceId id) { unloadDObject(id); },
        .dependencies = { data.data.aObject }
      });

      return id;
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    AObjectFactory& m_aObjectFactory;
    std::unordered_map<ResourceId, DObjectPtr> m_objects;

    // Called from resource manager thread
    void loadDObject(const Resource& resource)
    {
      auto& data = dynamic_cast<const DObjectDataWrapper&>(*resource.data);
      m_objects.insert({ resource.id, std::make_unique<DObject>(DObject{
        .whizz = data.data.whizz,
        .aObject = resource.dependencies[0]
      })});

      m_delegate.loadDObject();
    }

    // Called from resource manager thread
    void unloadDObject(ResourceId id)
    {
      m_objects.erase(id);

      m_delegate.unloadDObject();
    }
};

TEST_F(ResourceManagerTest, loadsAndUnloadsResource)
{
  NiceMock<MockLogger> logger;
  Delegate delegate;

  auto resourceManager = createResourceManager(logger);
  AObjectFactory factory{delegate, *resourceManager};

  testing::Sequence seq;
  EXPECT_CALL(delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadAObject()).Times(1).InSequence(seq);

  auto id = factory.createResource(std::make_unique<AObjectDataWrapper>(AObjectData{
    .foo = "hello"
  }));
  resourceManager->loadResources({ id });
  resourceManager->unloadResources({ id }).get();
}

TEST_F(ResourceManagerTest, loadsAndUnloadsTransitiveDependencies)
{
  NiceMock<MockLogger> logger;
  Delegate delegate;

  auto resourceManager = createResourceManager(logger);

  AObjectFactory aObjectFactory{delegate, *resourceManager};
  BObjectFactory bObjectFactory{delegate, *resourceManager, aObjectFactory};
  CObjectFactory cObjectFactory{delegate, *resourceManager, bObjectFactory};

  testing::Sequence seq;
  EXPECT_CALL(delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, loadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, loadCObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadCObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadAObject).Times(1).InSequence(seq);

  auto id = cObjectFactory.createResource(std::make_unique<CObjectDataWrapper>(CObjectData{
    .foo = "hello",
    .bar = 123,
    .baz = 234
  }));
  resourceManager->loadResources({ id });
  resourceManager->unloadResources({ id }).get();
}

TEST_F(ResourceManagerTest, doesNotUnloadStillUsedDependency)
{
  NiceMock<MockLogger> logger;
  Delegate delegate;

  auto resourceManager = createResourceManager(logger);

  AObjectFactory aObjectFactory{delegate, *resourceManager};
  BObjectFactory bObjectFactory{delegate, *resourceManager, aObjectFactory};
  DObjectFactory dObjectFactory{delegate, *resourceManager, aObjectFactory};

  testing::Sequence seq;
  EXPECT_CALL(delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, loadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, loadDObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadAObject).Times(0).InSequence(seq);

  auto bObjId = bObjectFactory.createResource(std::make_unique<BObjectDataWrapper>(BObjectData{
    .foo = "hello",
    .bar = 123
  }));

  resourceManager->loadResources({ bObjId }).get();
  auto bObject = bObjectFactory.get(bObjId);

  auto dObjId = dObjectFactory.createResource(std::make_unique<DObjectDataWrapper>(DObjectData{
    .aObject = bObject.aObject,
    .whizz = 'x'
  }));

  resourceManager->loadResources({ dObjId });
  resourceManager->unloadResources({ bObjId }).get();
}

TEST_F(ResourceManagerTest, unloadsSharedDependencyWhenNoLongerUsed)
{
  NiceMock<MockLogger> logger;
  Delegate delegate;

  auto resourceManager = createResourceManager(logger);

  AObjectFactory aObjectFactory{delegate, *resourceManager};
  BObjectFactory bObjectFactory{delegate, *resourceManager, aObjectFactory};
  DObjectFactory dObjectFactory{delegate, *resourceManager, aObjectFactory};

  testing::Sequence seq;
  EXPECT_CALL(delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, loadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, loadDObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadDObject).Times(1).InSequence(seq);
  EXPECT_CALL(delegate, unloadAObject).Times(1).InSequence(seq);

  auto bObjId = bObjectFactory.createResource(std::make_unique<BObjectDataWrapper>(BObjectData{
    .foo = "hello",
    .bar = 123
  }));

  resourceManager->loadResources({ bObjId }).get();
  auto bObject = bObjectFactory.get(bObjId);

  auto dObjId = dObjectFactory.createResource(std::make_unique<DObjectDataWrapper>(DObjectData{
    .aObject = bObject.aObject,
    .whizz = 'x'
  }));

  resourceManager->loadResources({ dObjId });
  resourceManager->unloadResources({ bObjId, dObjId }).get();
}
