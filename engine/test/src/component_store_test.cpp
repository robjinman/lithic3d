#include <lithic3d/component_store.hpp>
#include <lithic3d/sys_render_2d.hpp>
#include <gtest/gtest.h>
#include <vector>

using namespace lithic3d;

class EcsTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

struct CExample
{
  int a;
  int b;
  char c;
};

struct CExampleView
{
  int a;
  int b;
  uint8_t _padding[16];

  static constexpr size_t TypeId = 1 << 0;
};

class ExampleSystem
{
  public:
    ExampleSystem(ComponentStore& componentStore)
      : m_componentStore(componentStore) {}

    void addEntity(ResourceId id, const CExample& data)
    {
      m_componentStore.component<CExampleData>(id) = CExampleData{
        .a = data.a + data.c,
        .b = data.b,
        ._d{},
        ._e{}
      };
    }

  private:
    ComponentStore& m_componentStore;

    struct CExampleData
    {
      int a;
      int b;
      float _d;
      double _e;

      static constexpr size_t TypeId = 1 << 0;
    };

    static_assert(sizeof(CExampleData) == sizeof(CExampleView));
};

TEST_F(EcsTest, store_and_retrieve_single_component)
{
  ComponentStore componentStore;
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);

  ExampleSystem system{componentStore};

  auto entityId = idGen->getNewEntityId();
  componentStore.allocateEntity<CExampleView>(entityId);

  CExample example{
    .a = 1,
    .b = 2,
    .c = 'c'
  };

  system.addEntity(entityId, example);

  size_t i = 0;
  for (auto& arrayGroup : componentStore.components<CExampleView>()) {
    ASSERT_LT(i, 1);

    auto components = arrayGroup.components<CExampleView>();

    auto& entityIds = arrayGroup.entityIds();
    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entityId, entityIds[0]);

    EXPECT_EQ(1 + 'c', components[0].a);
    EXPECT_EQ(2, components[0].b);

    ++i;
  }
}

struct ComponentA
{
  static constexpr ComponentTypeId TypeId = 1 << 1;

  float a;
  float b;
};

struct ComponentB
{
  static constexpr ComponentTypeId TypeId = 1 << 2;

  int a;
  int b;
  double c;
};

struct ComponentC
{
  static constexpr ComponentTypeId TypeId = 1 << 3;

  short a;
  double b;
  long c;
  long d;
};

struct ComponentD
{
  static constexpr ComponentTypeId TypeId = 1 << 4;

  char a;
};

TEST_F(EcsTest, store_and_retrieve_2_components_of_1_entity)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  ComponentA componentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentB componentB{
    .a = 1,
    .b = 2,
    .c = 3.0
  };

  auto entityId = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA, ComponentB>(entityId);
  componentStore.component<ComponentA>(entityId) = componentA;
  componentStore.component<ComponentB>(entityId) = componentB;

  size_t i = 0;
  for (auto& group : componentStore.components<ComponentA, ComponentB>()) {
    ASSERT_LT(i, 1);

    auto aComponents = group.components<ComponentA>();
    auto bComponents = group.components<ComponentB>();

    auto& entityIds = group.entityIds();
    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entityId, entityIds[0]);

    EXPECT_EQ(1.f, aComponents[0].a);
    EXPECT_EQ(2.f, aComponents[0].b);

    EXPECT_EQ(1, bComponents[0].a);
    EXPECT_EQ(2, bComponents[0].b);
    EXPECT_EQ(3.0, bComponents[0].c);

    ++i;
  }
}

TEST_F(EcsTest, store_2_entities_with_single_component)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  ComponentA entity1ComponentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentA entity2ComponentA{
    .a = 3.f,
    .b = 4.f,
  };

  auto entity1 = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA>(entity1);
  auto entity2 = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA>(entity2);

  componentStore.component<ComponentA>(entity1) = entity1ComponentA;
  componentStore.component<ComponentA>(entity2) = entity2ComponentA;

  size_t i = 0;
  for (auto& group : componentStore.components<ComponentA>()) {
    ASSERT_LT(i, 1);

    auto& entityIds = group.entityIds();
    ASSERT_EQ(2, entityIds.size());
    EXPECT_EQ(entity1, entityIds[0]);
    EXPECT_EQ(entity2, entityIds[1]);

    std::vector<float> values;
    for (auto& c : group.components<ComponentA>()) {
      values.push_back(c.a);
      values.push_back(c.b);
    }

    EXPECT_EQ(1.f, values[0]);
    EXPECT_EQ(2.f, values[1]);
    EXPECT_EQ(3.f, values[2]);
    EXPECT_EQ(4.f, values[3]);

    ++i;
  }
}

TEST_F(EcsTest, store_2_entities_overlapping_archetypes)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  ComponentA entity1ComponentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentA entity2ComponentA{
    .a = 3.f,
    .b = 4.f,
  };

  ComponentB entity2ComponentB{
    .a = 5,
    .b = 6,
    .c = 7.0
  };

  auto entity1 = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA>(entity1);
  auto entity2 = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA, ComponentB>(entity2);

  componentStore.component<ComponentA>(entity1) = entity1ComponentA;
  componentStore.component<ComponentA>(entity2) = entity2ComponentA;
  componentStore.component<ComponentB>(entity2) = entity2ComponentB;

  auto view = componentStore.components<ComponentA>();
  auto i = view.begin();
  ASSERT_FALSE(i == view.end());

  {
    auto& group = *i;
    auto& entityIds = group.entityIds();

    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entity1, entityIds[0]);

    auto aComponents = group.components<ComponentA>();

    EXPECT_EQ(1.f, aComponents[0].a);
    EXPECT_EQ(2.f, aComponents[0].b);
  }

  ++i;
  ASSERT_FALSE(i == view.end());

  {
    auto& group = *i;
    auto& entityIds = group.entityIds();

    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entity2, entityIds[0]);

    auto aComponents = group.components<ComponentA>();
    auto bComponents = group.components<ComponentB>();

    EXPECT_EQ(3.f, aComponents[0].a);
    EXPECT_EQ(4.f, aComponents[0].b);

    EXPECT_EQ(5, bComponents[0].a);
    EXPECT_EQ(6, bComponents[0].b);
    EXPECT_EQ(7.0, bComponents[0].c);
  }

  ++i;
  EXPECT_TRUE(i == view.end());
}

TEST_F(EcsTest, get_entity_by_id)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  ComponentA entity1ComponentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentA entity2ComponentA{
    .a = 3.f,
    .b = 4.f,
  };

  ComponentB entity2ComponentB{
    .a = 5,
    .b = 6,
    .c = 7.0
  };

  auto entity1 = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA>(entity1);
  auto entity2 = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA, ComponentB>(entity2);

  componentStore.component<ComponentA>(entity1) = entity1ComponentA;
  componentStore.component<ComponentA>(entity2) = entity2ComponentA;
  componentStore.component<ComponentB>(entity2) = entity2ComponentB;

  auto& c = componentStore.component<ComponentA>(entity2);
  EXPECT_EQ(3.f, c.a);
  EXPECT_EQ(4.f, c.b);
}

TEST_F(EcsTest, remove_only_entity)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  ComponentA componentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentB componentB{
    .a = 1,
    .b = 2,
    .c = 3.0
  };

  auto entityId = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA, ComponentB>(entityId);

  componentStore.component<ComponentA>(entityId) = componentA;
  componentStore.component<ComponentB>(entityId) = componentB;

  auto view = componentStore.components<ComponentA, ComponentB>();
  auto& group = *view.begin();

  EXPECT_EQ(1, group.numEntities());

  componentStore.remove(entityId);

  EXPECT_EQ(0, group.numEntities());
}

TEST_F(EcsTest, cannot_modify_const_componentStore)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  ComponentA componentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentB componentB{
    .a = 1,
    .b = 2,
    .c = 3.0
  };

  auto entityId = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA, ComponentB>(entityId);

  componentStore.component<ComponentA>(entityId) = componentA;
  componentStore.component<ComponentB>(entityId) = componentB;

  auto tryModify = [entityId](const ComponentStore& cComponentStore) {
    auto view = cComponentStore.components<ComponentA, ComponentB>();
    //auto& group = *view.begin(); // Not allowed
    auto& group = *view.cbegin();

    auto& compA = group.component<ComponentA>(entityId);
    //compA.a = 123; // Not allowed

    auto compAs = group.components<ComponentA>();
    //compAs[0].a = 123; // Not allowed

    auto& compB = group.component<ComponentB>(entityId);

    // Reading is fine
    EXPECT_EQ(1.f, compA.a);
    EXPECT_EQ(2.f, compA.b);
    EXPECT_EQ(1, compB.a);
    EXPECT_EQ(2, compB.b);
    EXPECT_EQ(3.0, compB.c);
  };

  tryModify(componentStore);
}


TEST_F(EcsTest, hasComponentForEntity)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  auto entityId = idGen->getNewEntityId();
  componentStore.allocateEntity<ComponentA>(entityId);

  EXPECT_TRUE(componentStore.hasComponentForEntity<ComponentA>(entityId));
  EXPECT_FALSE(componentStore.hasComponentForEntity<ComponentB>(entityId));
}

TEST_F(EcsTest, duplicates_ok)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  componentStore.allocateEntity<ComponentA>(idGen->getNewEntityId());
  componentStore.allocateEntity<ComponentA, ComponentA>(idGen->getNewEntityId());

  auto compAView = componentStore.components<ComponentA>();

  // Should be only 1 group
  ASSERT_EQ(compAView.end(), ++compAView.begin());

  ASSERT_EQ(2, (*compAView.begin()).entityIds().size());
  ASSERT_EQ(2, (*compAView.begin()).components<ComponentA>().size());
}

TEST_F(EcsTest, order_invariant)
{
  auto idGen = createEntityIdAllocator(NULL_ENTITY_ID);
  ComponentStore componentStore;

  componentStore.allocateEntity<ComponentA, ComponentB>(idGen->getNewEntityId());
  componentStore.allocateEntity<ComponentB, ComponentA>(idGen->getNewEntityId());

  auto compAView = componentStore.components<ComponentA>();
  auto compBView = componentStore.components<ComponentB>();

  // Should be only 1 group
  ASSERT_EQ(compAView.end(), ++compAView.begin());
  ASSERT_EQ(compBView.end(), ++compBView.begin());

  auto aComps = (*compAView.begin()).components<ComponentA>();
  ASSERT_EQ(2, aComps.size());

  auto bComps = (*compBView.begin()).components<ComponentB>();
  ASSERT_EQ(2, bComps.size());
}
