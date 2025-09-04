#include "scene_builder.hpp"
#include "sys_animation.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
#include "ecs.hpp"
#include "b_collectable.hpp"
#include "b_numeric_tile.hpp"
#include "b_generic.hpp"
#include "game_events.hpp"
#include "player.hpp"
#include "systems.hpp"
#include "events.hpp"
#include <random>

namespace
{

// TODO: Move this
Mat4x4f spriteTransform(const Vec2f& pos, const Vec2f& size)
{
  return translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
    scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f });
}

class SceneBuilderImpl : public SceneBuilder
{
  public:
    SceneBuilderImpl(EventSystem& eventSystem, Ecs& ecs);

    Scene buildScene() override;
    EntityIdSet entities() const override;

  private:
    EventSystem& m_eventSystem;
    Ecs& m_ecs;
    EntityIdSet m_entities;
    EntityId m_worldRoot;
    EntityId m_menuRoot;
    EntityId m_playerId;

    EntityId constructWorldRoot();
    void constructSky();
    void constructClouds();
    void constructTrees();
    void constructFakeSoil();
    void constructSoil();
    std::set<std::pair<int, int>> constructMines();
    void constructNumbers(const std::set<std::pair<int, int>>& mines);
    void constructGradient();
    void constructCoins();
    void constructGoldNuggets(const std::set<std::pair<int, int>>& mines);
    void constructWanderers();
    void constructSticks();
    void constructHud();
};

SceneBuilderImpl::SceneBuilderImpl(EventSystem& eventSystem, Ecs& ecs)
  : m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

EntityIdSet SceneBuilderImpl::entities() const
{
  return m_entities;
}

Scene SceneBuilderImpl::buildScene()
{
  m_entities.clear();

  m_worldRoot = constructWorldRoot();
  m_playerId = constructPlayer(m_eventSystem, m_ecs, m_worldRoot);
  constructSky();
  constructClouds();
  constructTrees();
  constructFakeSoil();
  constructSoil();
  auto mines = constructMines();
  constructNumbers(mines);
  constructGradient();
  constructCoins();
  constructGoldNuggets(mines);
  constructWanderers();
  constructSticks();
  constructHud();

  Scene scene;
  scene.player = m_playerId;
  scene.worldRoot = m_worldRoot;

  return scene;
}

EntityId SceneBuilderImpl::constructWorldRoot()
{
  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  sysSpatial.addEntity(id, SpatialData{
    .transform = identityMatrix<float_t, 4>(),
    .parent = sysSpatial.root(),
    .enabled = true
  });

  return id;
}

void SceneBuilderImpl::constructSky()
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
  >();

  m_entities.insert(id);

  Vec2f size{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H };
  Vec2f pos{ 0.f, 12.f * GRID_CELL_H };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = Rectf{
      .x = pxToUvX(0.f),
      .y = pxToUvY(416.f, 32.f),
      .w = pxToUvW(128.f),
      .h = pxToUvH(32.f)
    },
    .zIndex = 0
  };

  sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructClouds()
{
  static auto strIdle = hashString("idle");

  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  long animationDuration = 7200;

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = hashString("idle"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ -2.f * GRID_W * GRID_CELL_W, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = std::nullopt
      }
    }
  });

  auto animIdleId = sysAnimation.addAnimation(std::move(animIdle));

  {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();

    m_entities.insert(id);

    Vec2f size{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H };
    Vec2f pos{ GRID_CELL_W * GRID_W, 0.8f };

    SpatialData spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
        .x = pxToUvX(256.f),
        .y = pxToUvY(0.f, 32.f),
        .w = pxToUvW(128.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = 1,
      .colour = Vec4f{ 1.f, 0.8f, 0.5f, 0.6f }
    };

    sysRender.addEntity(id, render);

    AnimationData animation{
      .animations = { animIdleId }
    };

    sysAnimation.addEntity(id, animation);

    sysAnimation.playAnimation(id, strIdle, true);
    sysAnimation.seek(id, animationDuration / 2);
  }

  {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();

    m_entities.insert(id);

    Vec2f size{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H };
    Vec2f pos{ GRID_CELL_W * GRID_W, 0.8f };

    SpatialData spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
        .x = pxToUvX(256.f),
        .y = pxToUvY(32.f, 32.f),
        .w = pxToUvW(128.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = 1,
      .colour = Vec4f{ 1.f, 0.8f, 0.5f, 0.6f }
    };

    sysRender.addEntity(id, render);

    AnimationData animation{
      .animations = { animIdleId }
    };

    sysAnimation.addEntity(id, animation);

    sysAnimation.playAnimation(id, strIdle, true);
  }
}

void SceneBuilderImpl::constructTrees()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
  >();
  
  m_entities.insert(id);

  Vec2f size{ GRID_CELL_W * GRID_W, 3.f * GRID_CELL_H };
  Vec2f pos{ 0.f, 11.f * GRID_CELL_H };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);
  
  SpriteData render{
    .textureRect = Rectf{
      .x = pxToUvX(0.f),
      .y = pxToUvY(352.f, 40.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(40.f)
    },
    .zIndex = 2
  };

  sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructFakeSoil()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  for (size_t i = 0; i < GRID_W; ++i) {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();
    
    m_entities.insert(id);

    float_t x = GRID_CELL_W * i;

    Vec2f size{ GRID_CELL_W, GRID_CELL_H };
    Vec2f pos{ x, 11.f * GRID_CELL_H };

    SpatialData spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
        .x = pxToUvX(384.f),
        .y = pxToUvY(0.f, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .zIndex = 1
    };

    sysRender.addEntity(id, render);
  }
}

void SceneBuilderImpl::constructSoil()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  long animationDuration = 16;

  auto animCollect = std::unique_ptr<Animation>(new Animation{
    .name = hashString("collect"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.875f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.75f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.625f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.5f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.375f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.25f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.125f }
      }
    }
  });

  auto animCollectId = sysAnimation.addAnimation(std::move(animCollect));

  for (size_t j = 0; j < GRID_H; ++j) {
    for (size_t i = 0; i < GRID_W; ++i) {
      if ((i < 2 && j < 2) || (i > GRID_W - 3 && j > GRID_H - 3)) {
        continue;
      }

      auto id = m_ecs.componentStore().allocate<
        CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
      >();
      
      m_entities.insert(id);

      sysGrid.addEntity(id, i, j);

      float_t x = GRID_CELL_W * i;
      float_t y = GRID_CELL_H * j;

      Vec2f size{ GRID_CELL_W, GRID_CELL_H };
      Vec2f pos{ x, y };

      SpatialData spatial{
        .transform = spriteTransform(pos, size),
        .parent = m_worldRoot,
        .enabled = true
      };

      sysSpatial.addEntity(id, spatial);

      SpriteData render{
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .zIndex = 1
      };

      sysRender.addEntity(id, render);

      sysAnimation.addEntity(id, AnimationData{
        .animations = { animCollectId }
      });

      auto behaviour = createBCollectable(m_ecs, m_eventSystem, id, m_playerId, 0);
      sysBehaviour.addBehaviour(id, std::move(behaviour));
    }
  }
}

std::set<std::pair<int, int>> SceneBuilderImpl::constructMines()
{
  static auto strExplode = hashString("explode");

  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  size_t numMines = 40; // TODO

  auto animExplode = std::unique_ptr<Animation>(new Animation{
    .name = hashString("explode"),
    .duration = 30,
    .startPos{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(576.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(640.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(576.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(640.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(576.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(640.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(192.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(192.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
        .scale = Vec2f{ 2.f, 2.f },
        .textureRect = Rectf{
          .x = pxToUvX(576.f),
          .y = pxToUvY(192.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      }
    }
  });

  auto animExplodeId = sysAnimation.addAnimation(std::move(animExplode));

  std::vector<Vec2i> coords;
  for (int j = 0; j < GRID_H; ++j) {
    for (int i = 0; i < GRID_W; ++i) {
      if ((i < 2 && j < 2) || (i > GRID_W - 3 && j > GRID_H - 3)) {
        continue;
      }

      coords.push_back(Vec2i{ i, j });
    }
  }

  std::random_device device;
  std::mt19937 generator(device());

  std::shuffle(coords.begin(), coords.end(), generator);

  std::set<std::pair<int, int>> mines;
  for (size_t i = 0; i < numMines; ++i) {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();

    m_entities.insert(id);

    int x = coords[i][0];
    int y = coords[i][1];

    sysGrid.addEntity(id, x, y);

    Vec2f size{ GRID_CELL_W, GRID_CELL_H };
    Vec2f pos{ GRID_CELL_W * x, GRID_CELL_H * y };

    SpatialData spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
        .x = pxToUvX(672.f),
        .y = pxToUvY(224.f, 32.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = 0
    };

    sysRender.addEntity(id, render);

    auto onStepOn = [&, this, id, x, y](const Event& e) {
      if (e.name == g_strEntityStepOn) {
        auto& event = dynamic_cast<const EEntityStepOn&>(e);

        Vec2i pos{ x, y };
        if (event.toPos == pos) {
          EntityIdSet targets = sysGrid.getEntities(x - 1, y - 1, x + 1, y + 1);

          m_eventSystem.queueEvent(std::make_unique<EEntityExplode>(id, pos, targets));

          sysAnimation.playAnimation(id, strExplode);
          sysRender.setZIndex(id, 100);
        }
      }
      else if (e.name == g_strAnimationFinish) {
        auto& event = dynamic_cast<const EAnimationFinish&>(e);

        if (event.entityId == id && event.animationName == strExplode) {
          m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(id));
        }
      }
    };

    auto explode = createBGeneric(strExplode, { g_strEntityStepOn }, onStepOn);
    sysBehaviour.addBehaviour(id, std::move(explode));

    sysAnimation.addEntity(id, AnimationData{
      .animations = { animExplodeId }
    });

    mines.insert({ x, y });
  }

  return mines;
}

void SceneBuilderImpl::constructNumbers(const std::set<std::pair<int, int>>& mines)
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  std::array<std::array<int, GRID_W>, GRID_H> numbers{};

  auto inRange = [](int x, int y) {
    return x >= 0 && x < GRID_W && y >= 0 && y < GRID_H;
  };

  for (auto& mine : mines) {
    for (int i = -1; i <= 1; ++i) {
      for (int j = -1; j <= 1; ++j) {
        int x = mine.first + i;
        int y = mine.second + j;

        if (inRange(x, y)) {
          ++numbers[y][x];
        }
      }
    }
  }

  for (int j = 0; j < GRID_H; ++j) {
    for (int i = 0; i < GRID_W; ++i) {
      int value = numbers[j][i];
      if (value == 0) {
        continue;
      }

      auto id = m_ecs.componentStore().allocate<
        CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
      >();

      m_entities.insert(id);

      sysGrid.addEntity(id, i, j);

      Vec2f size{ GRID_CELL_W, GRID_CELL_H };
      Vec2f pos{ GRID_CELL_W * i, GRID_CELL_H * j };

      SpatialData spatial{
        .transform = spriteTransform(pos, size),
        .parent = m_worldRoot,
        .enabled = true
      };

      sysSpatial.addEntity(id, spatial);

      SpriteData render{
        .textureRect = Rectf{
          .x = pxToUvX(16.f * (value - 1)),
          .y = pxToUvY(400.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .zIndex = 0
      };

      sysRender.addEntity(id, render);
      sysRender.setVisible(id, !mines.contains({ i, j }));

      auto behaviour = createBNumericTile(m_ecs, m_eventSystem, id, Vec2i{ i, j }, value);

      sysBehaviour.addBehaviour(id, std::move(behaviour));
    }
  }
}

void SceneBuilderImpl::constructGradient()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
  >();

  m_entities.insert(id);

  Vec2f size{ GRID_CELL_W * GRID_W, GRID_CELL_H * (5 + GRID_H) };
  Vec2f pos{ 0.f, 0.f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = Rectf{
      .x = pxToUvX(512.f),
      .y = pxToUvY(256.f, 128.f),
      .w = pxToUvW(128.f),
      .h = pxToUvH(128.f)
    },
    .zIndex = 9
  };

  sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructCoins()
{

}

void SceneBuilderImpl::constructGoldNuggets(const std::set<std::pair<int, int>>& mines)
{

}

void SceneBuilderImpl::constructWanderers()
{

}

void SceneBuilderImpl::constructSticks()
{

}

void SceneBuilderImpl::constructHud()
{

}

} // namespace

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, Ecs& ecs)
{
  return std::make_unique<SceneBuilderImpl>(eventSystem, ecs);  
}
