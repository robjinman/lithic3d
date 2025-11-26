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

struct AObject
{
  std::string foo;
};

using AObjectPtr = std::unique_ptr<AObject>;

class AObjectFactory
{
  public:
    AObjectFactory(Delegate& delegate, ResourceManager& resourceManager)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
    {}

    ResourceId createAObject(const std::string& foo)
    {
      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .loader = [=]() { loadAObject(id, foo); },  // Be sure to capture by value
        .unloader = [=]() { unloadAObject(id); },
        .dependencies = {}
      });

      return id;
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    std::unordered_map<ResourceId, AObjectPtr> m_objects;

    // Called from resource manager thread
    void loadAObject(ResourceId id, const std::string& foo)
    {
      m_objects.insert({ id, std::make_unique<AObject>(foo) });

      m_delegate.loadAObject();
    }

    // Called from resource manager thread
    void unloadAObject(ResourceId id)
    {
      m_objects.erase(id);

      m_delegate.unloadAObject();
    }
};

// Demonstrates a resource that depends on another resource, e.g. a material that contains a texture
struct BObject
{
  int bar = 0;
  ResourceId aObject = NULL_RESOURCE_ID;
};

using BObjectPtr = std::unique_ptr<BObject>;

class BObjectFactory
{
  public:
    BObjectFactory(Delegate& delegate, ResourceManager& resourceManager,
      AObjectFactory& aObjectFactory)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
      , m_aObjectFactory(aObjectFactory)
    {}

    ResourceId createBObject(const std::string& foo, int bar)
    {
      auto aObjId = m_aObjectFactory.createAObject(foo);

      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .loader = [=]() { loadBObject(id, aObjId, bar); },
        .unloader = [=]() { unloadBObject(id); },
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
    void loadBObject(ResourceId id, ResourceId aObject, int bar)
    {
      m_objects.insert({ id, std::make_unique<BObject>(BObject{
        .bar = bar,
        .aObject = aObject
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

struct CObject
{
  uint64_t baz = 0;
  ResourceId bObject = NULL_RESOURCE_ID;
};

using CObjectPtr = std::unique_ptr<CObject>;

class CObjectFactory
{
  public:
    CObjectFactory(Delegate& delegate, ResourceManager& resourceManager,
      BObjectFactory& bObjectFactory)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
      , m_bObjectFactory(bObjectFactory)
    {}

    ResourceId createCObject(const std::string& foo, int bar, uint64_t baz)
    {
      auto bObjId = m_bObjectFactory.createBObject(foo, bar);

      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .loader = [=]() { loadCObject(id, bObjId, baz); },
        .unloader = [=]() { unloadCObject(id); },
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
    void loadCObject(ResourceId id, ResourceId bObject, uint64_t baz)
    {
      m_objects.insert({ id, std::make_unique<CObject>(CObject{
        .baz = baz,
        .bObject = bObject
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

struct DObject
{
  char whizz = 0;
  ResourceId aObject = NULL_RESOURCE_ID;
};

using DObjectPtr = std::unique_ptr<DObject>;

class DObjectFactory
{
  public:
    DObjectFactory(Delegate& delegate, ResourceManager& resourceManager,
      AObjectFactory& aObjectFactory)
      : m_delegate(delegate)
      , m_resourceManager(resourceManager)
      , m_aObjectFactory(aObjectFactory)
    {}

    // Demonstrates creation of a resource that is dependent on an already existing resource
    ResourceId createDObject(ResourceId aObject, char whizz)
    {
      auto id = m_resourceManager.nextResourceId();

      m_resourceManager.addResource(Resource{
        .id = id,
        .loader = [=]() { loadDObject(id, aObject, whizz); },
        .unloader = [=]() { unloadDObject(id); },
        .dependencies = { aObject }
      });

      return id;
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    AObjectFactory& m_aObjectFactory;
    std::unordered_map<ResourceId, DObjectPtr> m_objects;

    // Called from resource manager thread
    void loadDObject(ResourceId id, ResourceId aObject, char whizz)
    {
      m_objects.insert({ id, std::make_unique<DObject>(DObject{
        .whizz = whizz,
        .aObject = aObject
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

  auto id = factory.createAObject("hello");
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

  auto id = cObjectFactory.createCObject("hello", 123, 234);
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

  auto bObjId = bObjectFactory.createBObject("hello", 123);

  resourceManager->loadResources({ bObjId }).get();
  auto bObject = bObjectFactory.get(bObjId);

  auto dObjId = dObjectFactory.createDObject(bObject.aObject, 'x');

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

  auto bObjId = bObjectFactory.createBObject("hello", 123);

  resourceManager->loadResources({ bObjId }).get();
  auto bObject = bObjectFactory.get(bObjId);

  auto dObjId = dObjectFactory.createDObject(bObject.aObject, 'x');

  resourceManager->loadResources({ dObjId });
  resourceManager->unloadResources({ bObjId, dObjId }).get();
}
