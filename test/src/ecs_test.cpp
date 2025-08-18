#include <ecs.hpp>
#include <gtest/gtest.h>
#include <vector>
#include "sys_render.hpp"

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
    ExampleSystem(World& world)
      : m_world(world) {}

    void addEntity(EntityId id, const CExample& data)
    {
      m_world.component<CExampleData>(id) = CExampleData{
        .a = data.a + data.c,
        .b = data.b
      };
    }

  private:
    World& m_world;

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
  World world;

  ExampleSystem system{world};

  auto entityId = world.allocate<CExampleView>();

  CExample example{
    .a = 1,
    .b = 2,
    .c = 'c'
  };

  system.addEntity(entityId, example);

  auto arrayGroups = world.components<CExampleView>();
  ASSERT_EQ(1, arrayGroups.size());

  auto components = arrayGroups[0]->components<CExampleView>();

  auto& entityIds = arrayGroups[0]->entityIds();
  ASSERT_EQ(1, entityIds.size());
  EXPECT_EQ(entityId, entityIds[0]);

  EXPECT_EQ(1 + 'c', components[0].a);
  EXPECT_EQ(2, components[0].b);
}

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

  auto entityId = world.allocate<ComponentA, ComponentB>();
  world.component<ComponentA>(entityId) = componentA;
  world.component<ComponentB>(entityId) = componentB;

  auto arrayGroups = world.components<ComponentA, ComponentB>();

  ASSERT_EQ(1, arrayGroups.size());

  auto aComponents = arrayGroups[0]->components<ComponentA>();
  auto bComponents = arrayGroups[0]->components<ComponentB>();

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

  auto entity1 = world.allocate<ComponentA>();
  auto entity2 = world.allocate<ComponentA>();

  world.component<ComponentA>(entity1) = entity1ComponentA;
  world.component<ComponentA>(entity2) = entity2ComponentA;

  auto arrayGroups = world.components<ComponentA>();

  auto& entityIds = arrayGroups[0]->entityIds();
  ASSERT_EQ(2, entityIds.size());
  EXPECT_EQ(entity1, entityIds[0]);
  EXPECT_EQ(entity2, entityIds[1]);

  auto aComponents = arrayGroups[0]->components<ComponentA>();

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

  auto entity1 = world.allocate<ComponentA>();
  auto entity2 = world.allocate<ComponentA, ComponentB>();

  world.component<ComponentA>(entity1) = entity1ComponentA;
  world.component<ComponentA>(entity2) = entity2ComponentA;
  world.component<ComponentB>(entity2) = entity2ComponentB;

  auto arrayGroups = world.components<ComponentA>();
  ASSERT_EQ(2, arrayGroups.size());

  {
    size_t group = 0;

    auto& entityIds = arrayGroups[group]->entityIds();
    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entity1, entityIds[0]);

    auto aComponents = arrayGroups[group]->components<ComponentA>();

    EXPECT_EQ(1.f, aComponents[0].a);
    EXPECT_EQ(2.f, aComponents[0].b);
  }

  {
    size_t group = 1;

    auto& entityIds = arrayGroups[group]->entityIds();
    ASSERT_EQ(1, entityIds.size());
    EXPECT_EQ(entity2, entityIds[0]);

    auto aComponents = arrayGroups[group]->components<ComponentA>();
    auto bComponents = arrayGroups[group]->components<ComponentB>();

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

  auto entity1 = world.allocate<ComponentA>();
  auto entity2 = world.allocate<ComponentA, ComponentB>();

  world.component<ComponentA>(entity1) = entity1ComponentA;
  world.component<ComponentA>(entity2) = entity2ComponentA;
  world.component<ComponentB>(entity2) = entity2ComponentB;

  auto& c = world.component<ComponentA>(entity2);
  EXPECT_EQ(3.f, c.a);
  EXPECT_EQ(4.f, c.b);
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

  auto entityId = world.allocate<ComponentA, ComponentB>();

  world.component<ComponentA>(entityId) = componentA;
  world.component<ComponentB>(entityId) = componentB;

  auto arrayGroups = world.components<ComponentA, ComponentB>();

  ASSERT_EQ(1, arrayGroups.size());
  EXPECT_EQ(1, arrayGroups[0]->numEntities());

  world.remove(entityId);

  EXPECT_EQ(0, arrayGroups[0]->numEntities());
}
