#include "mock_logger.hpp"
#include <lithic3d/resource_manager.hpp>
#include <gtest/gtest.h>
#include <vector>

using testing::NiceMock;
using testing::Return;
using namespace lithic3d;

class ResourceManagerTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

class Factory {
 public:
  MOCK_METHOD(std::string, createAResource, ());
  MOCK_METHOD(void, deleteAResource, (std::string));

  MOCK_METHOD(int, createBResource, ());
  MOCK_METHOD(void, deleteBResource, (int));

  MOCK_METHOD(uint64_t, createCResource, ());
  MOCK_METHOD(void, deleteCResource, (uint64_t));
};

template<typename T>
struct Handle : public ResourceHandle
{
  Handle(const T& h)
    : handle(h)
  {}

  T handle;
};

using HandleA = Handle<std::string>;
using HandleB = Handle<int>;
using HandleC = Handle<uint64_t>;

TEST_F(ResourceManagerTest, loadsAndUnloadsResource)
{
  NiceMock<MockLogger> logger;

  auto resourceManager = createResourceManager(logger);

  Factory factory;

  Resource resource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      auto handle = factory.createAResource();
      return std::make_unique<HandleA>(handle);
    },
    .unloader = [&factory](const ResourceHandle& h) {
      auto handle = dynamic_cast<const HandleA&>(h);
      factory.deleteAResource(handle.handle);
    },
    .dependencies = {}
  };

  testing::Sequence seq;
  EXPECT_CALL(factory, createAResource).Times(1).InSequence(seq).WillOnce(Return("handle"));
  EXPECT_CALL(factory, deleteAResource("handle")).Times(1).InSequence(seq);

  resourceManager->addResource(resource);
  resourceManager->loadResources({ resource.id });
  resourceManager->unloadResources({ resource.id }).get();
}

TEST_F(ResourceManagerTest, loadsTransitiveDependencies)
{
  NiceMock<MockLogger> logger;

  auto resourceManager = createResourceManager(logger);

  Factory factory;

  Resource subsubresource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleC>(factory.createCResource());
    },
    .unloader = [&factory](const ResourceHandle&) {},
    .dependencies = {}
  };

  uint64_t capturedSubsubresourceHandle = 0;
  Resource subresource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory, &capturedSubsubresourceHandle](const ResourceHandleList& dependencies) {
      capturedSubsubresourceHandle = dynamic_cast<const HandleC&>(dependencies[0].get()).handle;
      return std::make_unique<HandleB>(factory.createBResource());
    },
    .unloader = [&factory](const ResourceHandle&) {},
    .dependencies = { subsubresource.id }
  };

  int capturedSubresourceHandle = 0;
  Resource resource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory, &capturedSubresourceHandle](const ResourceHandleList& dependencies) {
      capturedSubresourceHandle = dynamic_cast<const HandleB&>(dependencies[0].get()).handle;
      return std::make_unique<HandleA>(factory.createAResource());
    },
    .unloader = [&factory](const ResourceHandle&) {},
    .dependencies = { subresource.id }
  };

  testing::Sequence seq;
  EXPECT_CALL(factory, createCResource).Times(1).InSequence(seq).WillOnce(Return(234));
  EXPECT_CALL(factory, createBResource).Times(1).InSequence(seq).WillOnce(Return(123));
  EXPECT_CALL(factory, createAResource).Times(1).InSequence(seq).WillOnce(Return("handle"));

  resourceManager->addResource(subsubresource);
  resourceManager->addResource(subresource);
  resourceManager->addResource(resource);

  resourceManager->loadResources({ resource.id }).get();

  EXPECT_EQ(capturedSubresourceHandle, 123);
  EXPECT_EQ(capturedSubsubresourceHandle, 234);
}

TEST_F(ResourceManagerTest, unloadsTransitiveDependencies)
{
  NiceMock<MockLogger> logger;

  auto resourceManager = createResourceManager(logger);

  Factory factory;

  Resource subsubresource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleC>(factory.createCResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteCResource(dynamic_cast<const HandleC&>(h).handle);
    },
    .dependencies = {}
  };

  Resource subresource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleB>(factory.createBResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteBResource(dynamic_cast<const HandleB&>(h).handle);
    },
    .dependencies = { subsubresource.id }
  };

  Resource resource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleA>(factory.createAResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteAResource(dynamic_cast<const HandleA&>(h).handle);
    },
    .dependencies = { subresource.id }
  };

  testing::Sequence seq;
  EXPECT_CALL(factory, createCResource).Times(1).InSequence(seq).WillOnce(Return(234));
  EXPECT_CALL(factory, createBResource).Times(1).InSequence(seq).WillOnce(Return(123));
  EXPECT_CALL(factory, createAResource).Times(1).InSequence(seq).WillOnce(Return("handle"));
  EXPECT_CALL(factory, deleteAResource("handle")).Times(1).InSequence(seq);
  EXPECT_CALL(factory, deleteBResource(123)).Times(1).InSequence(seq);
  EXPECT_CALL(factory, deleteCResource(234)).Times(1).InSequence(seq);

  resourceManager->addResource(subsubresource);
  resourceManager->addResource(subresource);
  resourceManager->addResource(resource);

  resourceManager->loadResources({ resource.id });
  resourceManager->unloadResources({ resource.id }).get();
}

TEST_F(ResourceManagerTest, doesNotUnloadStillUsedDependency)
{
  NiceMock<MockLogger> logger;

  auto resourceManager = createResourceManager(logger);

  Factory factory;

  Resource sharedResource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleC>(factory.createCResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteCResource(dynamic_cast<const HandleC&>(h).handle);
    },
    .dependencies = {}
  };

  Resource resource1{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleB>(factory.createBResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteBResource(dynamic_cast<const HandleB&>(h).handle);
    },
    .dependencies = { sharedResource.id }
  };

  Resource resource2{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleA>(factory.createAResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteAResource(dynamic_cast<const HandleA&>(h).handle);
    },
    .dependencies = { sharedResource.id }
  };

  testing::Sequence seq;
  EXPECT_CALL(factory, createCResource).Times(1).InSequence(seq).WillOnce(Return(234));       // sharedResource
  EXPECT_CALL(factory, createBResource).Times(1).InSequence(seq).WillOnce(Return(123));       // resource1
  EXPECT_CALL(factory, createAResource).Times(1).InSequence(seq).WillOnce(Return("handle"));  // resource2
  EXPECT_CALL(factory, deleteBResource(123)).Times(1).InSequence(seq);                        // resource1
  EXPECT_CALL(factory, deleteAResource("handle")).Times(0);                                   // resource2
  EXPECT_CALL(factory, deleteCResource(234)).Times(0);                                        // sharedResource

  resourceManager->addResource(sharedResource);
  resourceManager->addResource(resource1);
  resourceManager->addResource(resource2);

  resourceManager->loadResources({ resource1.id, resource2.id });
  resourceManager->unloadResources({ resource1.id }).get();
}

TEST_F(ResourceManagerTest, unloadsSharedDependencyWhenNoLongerUsed)
{
  NiceMock<MockLogger> logger;

  auto resourceManager = createResourceManager(logger);

  Factory factory;

  Resource sharedResource{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleC>(factory.createCResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteCResource(dynamic_cast<const HandleC&>(h).handle);
    },
    .dependencies = {}
  };

  Resource resource1{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleB>(factory.createBResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteBResource(dynamic_cast<const HandleB&>(h).handle);
    },
    .dependencies = { sharedResource.id }
  };

  Resource resource2{
    .id = resourceManager->nextResourceId(),
    .loader = [&factory](const ResourceHandleList&) {
      return std::make_unique<HandleA>(factory.createAResource());
    },
    .unloader = [&factory](const ResourceHandle& h) {
      factory.deleteAResource(dynamic_cast<const HandleA&>(h).handle);
    },
    .dependencies = { sharedResource.id }
  };

  testing::Sequence seq;
  EXPECT_CALL(factory, createCResource).Times(1).InSequence(seq).WillOnce(Return(234));       // sharedResource
  EXPECT_CALL(factory, createBResource).Times(1).InSequence(seq).WillOnce(Return(123));       // resource1
  EXPECT_CALL(factory, createAResource).Times(1).InSequence(seq).WillOnce(Return("handle"));  // resource2
  EXPECT_CALL(factory, deleteBResource(123)).Times(1).InSequence(seq);                        // resource1
  EXPECT_CALL(factory, deleteAResource("handle")).Times(1).InSequence(seq);                   // resource2
  EXPECT_CALL(factory, deleteCResource(234)).Times(1).InSequence(seq);                        // sharedResource

  resourceManager->addResource(sharedResource);
  resourceManager->addResource(resource1);
  resourceManager->addResource(resource2);

  resourceManager->loadResources({ resource1.id, resource2.id });
  resourceManager->unloadResources({ resource1.id, resource2.id }).get();
}
