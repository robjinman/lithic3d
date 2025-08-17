#include <ecs.hpp>
#include <gtest/gtest.h>
#include <vector>

class EcsTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

struct ComponentA
{
  static constexpr ComponentType TypeId = 1 << 1;

  float a;
  float b;
};

struct ComponentB
{
  static constexpr ComponentType TypeId = 1 << 2;

  int a;
  int b;
  double c;
};

struct ComponentC
{
  static constexpr ComponentType TypeId = 1 << 3;

  short a;
  double b;
  long c;
  long d;
};

struct ComponentD
{
  static constexpr ComponentType TypeId = 1 << 4;

  char a;
};

TEST_F(EcsTest, store_and_retrieve_single_component)
{
  World world;

  ComponentA componentA{
    .a = 1.f,
    .b = 2.f,
  };

  auto entityId = world.add(componentA);

  auto arrayGroups = world.get<ComponentA>();
  ASSERT_EQ(1, arrayGroups.size());

  auto components = arrayGroups[0]->get<ComponentA>();

  auto& entityIds = arrayGroups[0]->entityIds();
  ASSERT_EQ(1, entityIds.size());
  EXPECT_EQ(entityId, entityIds[0]);

  EXPECT_EQ(1.f, components[0].a);
  EXPECT_EQ(2.f, components[0].b);
}

TEST_F(EcsTest, store_and_retrieve_2_components_of_1_entity)
{
  World world;

  ComponentA componentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentB componentB{
    .a = 1,
    .b = 2,
    .c = 3.0
  };

  auto entityId = world.add(componentA, componentB);

  auto arrayGroups = world.get<ComponentA, ComponentB>();

  ASSERT_EQ(1, arrayGroups.size());

  auto aComponents = arrayGroups[0]->get<ComponentA>();
  auto bComponents = arrayGroups[0]->get<ComponentB>();

  auto& entityIds = arrayGroups[0]->entityIds();
  ASSERT_EQ(1, entityIds.size());
  EXPECT_EQ(entityId, entityIds[0]);

  EXPECT_EQ(1.f, aComponents[0].a);
  EXPECT_EQ(2.f, aComponents[0].b);

  EXPECT_EQ(1, bComponents[0].a);
  EXPECT_EQ(2, bComponents[0].b);
  EXPECT_EQ(3.0, bComponents[0].c);
}

TEST_F(EcsTest, store_2_entities_with_single_component)
{
  World world;

  ComponentA entity1ComponentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentA entity2ComponentA{
    .a = 3.f,
    .b = 4.f,
  };

  auto entity1 = world.add(entity1ComponentA);
  auto entity2 = world.add(entity2ComponentA);

  auto arrayGroups = world.get<ComponentA>();

  auto& entityIds = arrayGroups[0]->entityIds();
  ASSERT_EQ(2, entityIds.size());
  EXPECT_EQ(entity1, entityIds[0]);
  EXPECT_EQ(entity2, entityIds[1]);

  auto aComponents = arrayGroups[0]->get<ComponentA>();

  EXPECT_EQ(1.f, aComponents[0].a);
  EXPECT_EQ(2.f, aComponents[0].b);

  EXPECT_EQ(3.f, aComponents[1].a);
  EXPECT_EQ(4.f, aComponents[1].b);
}

TEST_F(EcsTest, store_2_entities_overlapping_archetypes)
{
  World world;

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

  auto entity1 = world.add(entity1ComponentA);
  auto entity2 = world.add(entity2ComponentA, entity2ComponentB);

  auto arrayGroups = world.get<ComponentA>();
  ASSERT_EQ(2, arrayGroups.size());

  {
    size_t group = 0;

    auto& entityIds = arrayGroups[group]->entityIds();
    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entity1, entityIds[0]);

    auto aComponents = arrayGroups[group]->get<ComponentA>();

    EXPECT_EQ(1.f, aComponents[0].a);
    EXPECT_EQ(2.f, aComponents[0].b);
  }

  {
    size_t group = 1;

    auto& entityIds = arrayGroups[group]->entityIds();
    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entity2, entityIds[0]);

    auto aComponents = arrayGroups[group]->get<ComponentA>();
    auto bComponents = arrayGroups[group]->get<ComponentB>();

    EXPECT_EQ(3.f, aComponents[0].a);
    EXPECT_EQ(4.f, aComponents[0].b);

    EXPECT_EQ(5, bComponents[0].a);
    EXPECT_EQ(6, bComponents[0].b);
    EXPECT_EQ(7.0, bComponents[0].c);
  }
}

TEST_F(EcsTest, get_entity_by_id)
{
  World world;

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

  auto entity1 = world.add(entity1ComponentA);
  auto entity2 = world.add(entity2ComponentA, entity2ComponentB);

  auto arrayGroups = world.get<ComponentA>();
  ASSERT_EQ(2, arrayGroups.size());

  size_t entity2Idx = arrayGroups[1]->entityPosition(entity2);
  EXPECT_EQ(0, entity2Idx);
}

TEST_F(EcsTest, remove_only_entity)
{
  World world;

  ComponentA componentA{
    .a = 1.f,
    .b = 2.f,
  };

  ComponentB componentB{
    .a = 1,
    .b = 2,
    .c = 3.0
  };

  auto entityId = world.add(componentA, componentB);

  auto arrayGroups = world.get<ComponentA, ComponentB>();
  ASSERT_EQ(1, arrayGroups.size());

  auto aComponents = arrayGroups[0]->get<ComponentA>();
  auto bComponents = arrayGroups[0]->get<ComponentB>();

  EXPECT_EQ(1, arrayGroups[0]->numEntities());

  world.remove(entityId);

  EXPECT_EQ(0, arrayGroups[0]->numEntities());
}
