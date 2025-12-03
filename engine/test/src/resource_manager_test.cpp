#include "mock_logger.hpp"
#include <lithic3d/resource_manager.hpp>
#include <gtest/gtest.h>
#include <vector>
#include <unordered_map>

using testing::NiceMock;
using testing::Return;
using namespace lithic3d;

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

    MOCK_METHOD(void, testComplete, ());
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

    [[nodiscard]] ResourceHandle createAObjectAsync(const std::string& foo)
    {
      return m_resourceManager.loadResource([this, foo](ResourceId id) {
        loadAObject(id, foo);

        return ManagedResource{
          .unloader = [this](ResourceId id) { unloadAObject(id); }
        };
      });
    }

    ~AObjectFactory()
    {
      m_resourceManager.waitAll();
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    std::unordered_map<ResourceId, AObjectPtr> m_objects;

    // Called from resource manager thread
    void loadAObject(ResourceId id, const std::string& foo)
    {
      m_delegate.loadAObject();
      m_objects.insert({ id, std::make_unique<AObject>(foo) });
    }

    // Called from resource manager thread
    void unloadAObject(ResourceId id)
    {
      m_delegate.unloadAObject();
      m_objects.erase(id);
    }
};

// Demonstrates a resource that depends on another resource, e.g. a material that contains a texture
struct BObject
{
  int bar = 0;
  ResourceHandle aObject;
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

    ResourceHandle createBObjectAsync(const std::string& foo, int bar)
    {
      return m_resourceManager.loadResource([this, foo, bar](ResourceId id) {
        auto aObjHandle = m_aObjectFactory.createAObjectAsync(foo);

        loadBObject(id, aObjHandle, bar);

        return ManagedResource{
          .unloader = [this](ResourceId id) { unloadBObject(id); }
        };
      });
    }

    const BObject& get(ResourceId id) const
    {
      return *m_objects.at(id);
    }

    ~BObjectFactory()
    {
      m_resourceManager.waitAll();
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    AObjectFactory& m_aObjectFactory;
    std::unordered_map<ResourceId, BObjectPtr> m_objects;

    // Called from resource manager thread
    void loadBObject(ResourceId id, ResourceHandle aObject, int bar)
    {
      m_delegate.loadBObject();
      m_objects.insert({ id, std::make_unique<BObject>(BObject{
        .bar = bar,
        .aObject = aObject
      })});
    }

    // Called from resource manager thread
    void unloadBObject(ResourceId id)
    {
      m_delegate.unloadBObject();
      m_objects.erase(id);
    }
};

struct CObject
{
  uint64_t baz = 0;
  ResourceHandle bObject;
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

    ResourceHandle createCObjectAsync(const std::string& foo, int bar, uint64_t baz)
    {
      return m_resourceManager.loadResource([this, foo, bar, baz](ResourceId id) {
        auto bObject = m_bObjectFactory.createBObjectAsync(foo, bar);

        loadCObject(id, bObject, baz);

        return ManagedResource{
          .unloader = [this](ResourceId id) { unloadCObject(id); }
        };
      });
    }

    ~CObjectFactory()
    {
      m_resourceManager.waitAll();
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    BObjectFactory& m_bObjectFactory;
    std::unordered_map<ResourceId, CObjectPtr> m_objects;

    // Called from resource manager thread
    void loadCObject(ResourceId id, ResourceHandle bObject, uint64_t baz)
    {
      m_delegate.loadCObject();
      m_objects.insert({ id, std::make_unique<CObject>(CObject{
        .baz = baz,
        .bObject = bObject
      })});
    }

    // Called from resource manager thread
    void unloadCObject(ResourceId id)
    {
      m_delegate.unloadCObject();
      m_objects.erase(id);
    }
};

struct DObject
{
  char whizz = 0;
  ResourceHandle aObject;
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
    ResourceHandle createDObjectAsync(ResourceHandle aObject, char whizz)
    {
      return m_resourceManager.loadResource([this, aObject, whizz](ResourceId id) {
        loadDObject(id, aObject, whizz);

        return ManagedResource{
          .unloader = [this](ResourceId id) { unloadDObject(id); }
        };
      });
    }

    ~DObjectFactory()
    {
      m_resourceManager.waitAll();
    }

  private:
    Delegate& m_delegate;
    ResourceManager& m_resourceManager;
    AObjectFactory& m_aObjectFactory;
    std::unordered_map<ResourceId, DObjectPtr> m_objects;

    // Called from resource manager thread
    void loadDObject(ResourceId id, ResourceHandle aObject, char whizz)
    {
      m_delegate.loadDObject();
      m_objects.insert({ id, std::make_unique<DObject>(DObject{
        .whizz = whizz,
        .aObject = aObject
      })});
    }

    // Called from resource manager thread
    void unloadDObject(ResourceId id)
    {
      m_delegate.unloadDObject();
      m_objects.erase(id);
    }
};

class ResourceManagerTest : public testing::Test
{
  public:
    virtual void SetUp() override
    {
      m_delegate = std::make_unique<Delegate>();
      m_realLogger = createLogger(std::cout, std::cout, std::cout, std::cout);
      m_resourceManager = createResourceManager(m_mockLogger);
      m_aObjectFactory = std::make_unique<AObjectFactory>(*m_delegate, *m_resourceManager);
      m_bObjectFactory = std::make_unique<BObjectFactory>(*m_delegate, *m_resourceManager,
        *m_aObjectFactory);
      m_cObjectFactory = std::make_unique<CObjectFactory>(*m_delegate, *m_resourceManager,
        *m_bObjectFactory);
      m_dObjectFactory = std::make_unique<DObjectFactory>(*m_delegate, *m_resourceManager,
        *m_aObjectFactory);
    }

    virtual void TearDown() override
    {
    }

  protected:
    std::unique_ptr<Logger> m_realLogger;
    NiceMock<MockLogger> m_mockLogger;
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<Delegate> m_delegate;
    std::unique_ptr<AObjectFactory> m_aObjectFactory;
    std::unique_ptr<BObjectFactory> m_bObjectFactory;
    std::unique_ptr<CObjectFactory> m_cObjectFactory;
    std::unique_ptr<DObjectFactory> m_dObjectFactory;
};

TEST_F(ResourceManagerTest, loadsAndUnloadsResource)
{
  testing::Sequence seq;
  EXPECT_CALL(*m_delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadAObject()).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, testComplete).Times(1).InSequence(seq);

  try {
    auto handle = m_aObjectFactory->createAObjectAsync("hello");
  }
  catch(...) {
    m_resourceManager->waitAll();
    throw;
  }

  m_resourceManager->waitAll();
  m_delegate->testComplete();
}

TEST_F(ResourceManagerTest, loadsAndUnloadsTransitiveDependencies)
{
  testing::Sequence seq;
  EXPECT_CALL(*m_delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, loadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, loadCObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadCObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, testComplete).Times(1).InSequence(seq);

  try {
    m_cObjectFactory->createCObjectAsync("hello", 123, 234).id();
  }
  catch(...) {
    m_resourceManager->waitAll();
    throw;
  }

  m_resourceManager->waitAll();
  m_delegate->testComplete();
}

TEST_F(ResourceManagerTest, doesNotUnloadStillUsedDependency)
{
  testing::Sequence seq;
  EXPECT_CALL(*m_delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, loadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, loadDObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, testComplete).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadDObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadAObject).Times(1).InSequence(seq);

  ResourceHandle tmpHandle;

  try {
    auto bObjHandle = m_bObjectFactory->createBObjectAsync("hello", 123);
    bObjHandle.wait();

    auto& bObject = m_bObjectFactory->get(bObjHandle.id());

    auto dObjHandle = m_dObjectFactory->createDObjectAsync(bObject.aObject, 'x');
    dObjHandle.wait();

    // Keep alive
    tmpHandle = dObjHandle;

    // bObjHandle goes out of scope here
  }
  catch(...) {
    m_resourceManager->waitAll();
    throw;
  }

  m_resourceManager->waitAll();
  m_delegate->testComplete();
}

TEST_F(ResourceManagerTest, unloadsSharedDependencyWhenNoLongerUsed)
{
  testing::Sequence seq;
  EXPECT_CALL(*m_delegate, loadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, loadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, loadDObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadDObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadBObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, unloadAObject).Times(1).InSequence(seq);
  EXPECT_CALL(*m_delegate, testComplete).Times(1).InSequence(seq);

  try {
    auto bObjHandle = m_bObjectFactory->createBObjectAsync("hello", 123);
    bObjHandle.wait();

    auto& bObject = m_bObjectFactory->get(bObjHandle.id());

    auto dObjHandle = m_dObjectFactory->createDObjectAsync(bObject.aObject, 'x');
    dObjHandle.wait();

    // dObjHandle goes out of scope here
    // bObjHandle goes out of scope here
  }
  catch(...) {
    m_resourceManager->waitAll();
    throw;
  }

  m_resourceManager->waitAll();
  m_delegate->testComplete();
}
