#include "scene_builder.hpp"
#include "sys_animation.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
#include "ecs.hpp"
#include "b_player.hpp"
#include "b_collectable.hpp"
#include "b_numeric_tile.hpp"
#include "b_generic.hpp"
#include "b_wanderer.hpp"
#include "b_coin_counter.hpp"
#include "b_stick.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include "events.hpp"
#include <random>
#include <iomanip>
#include <cstring>

namespace
{

enum class ZIndex : uint32_t
{
  NumericTile,
  Mine,
  Nugget,
  Soil,
  Exit,
  Sky,
  Cloud,
  Trees,
  Gradient,
  Stick,
  Coin,
  Player,
  Wanderer,
  Explosion,
  Hud
};

// TODO: Move this
Mat4x4f spriteTransform(const Vec2f& pos, const Vec2f& size, float_t rotation = 0.f,
  Vec2f pivot = Vec2f{})
{
  Vec3f scaledPivot{ size[0] * pivot[0], size[1] * pivot[1], 0.f };

  return
    translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
    translationMatrix4x4(scaledPivot) *
    rotationMatrix4x4(Vec3f{ 0.f, 0.f, rotation }) *
    translationMatrix4x4(-scaledPivot) *
    scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f });
}

std::vector<Vec2i> randomGridCoords()
{
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

  return coords;
}

int randomInt()
{
  std::random_device device;
  std::mt19937 generator(device());
  std::uniform_int_distribution distribution;

  return distribution(generator);
}

class SceneBuilderImpl : public SceneBuilder
{
  public:
    SceneBuilderImpl(EventSystem& eventSystem, Ecs& ecs, TimeService& timeService);

    Scene buildScene() override;
    EntityIdSet entities() const override;

  private:
    EventSystem& m_eventSystem;
    Ecs& m_ecs;
    TimeService& m_timeService;
    EntityIdSet m_entities;
    EntityId m_worldRoot;
    EntityId m_menuRoot;
    EntityId m_playerId;

    EntityId constructStaticEntity(const Vec2f& pos, const Vec2f& size, const Rectf& texRect,
      uint32_t zIndex);
    EntityId constructWorldRoot();
    EntityId constructPlayer();
    void constructSky();
    void constructClouds();
    void constructTrees();
    void constructFakeSoil();
    void constructSoil();
    std::set<std::pair<int, int>> constructMines();
    void constructNumericTiles(const std::set<std::pair<int, int>>& mines);
    void constructGradient();
    void constructCoins();
    void constructGoldNuggets(const std::set<std::pair<int, int>>& mines);
    void constructWanderers();
    void constructSticks();
    void constructExit();
    void constructCoinLabel();
    void constructCoinCounter();
    void constructTimeLabel();
    void constructTimeCounter();
    EntityId constructThrowingModeIndicator();
};

SceneBuilderImpl::SceneBuilderImpl(EventSystem& eventSystem, Ecs& ecs, TimeService& timeService)
  : m_eventSystem(eventSystem)
  , m_ecs(ecs)
  , m_timeService(timeService)
{
}

EntityIdSet SceneBuilderImpl::entities() const
{
  return m_entities;
}

Scene SceneBuilderImpl::buildScene()
{
  m_entities.clear();

  Scene scene;

  m_worldRoot = constructWorldRoot();
  m_playerId = constructPlayer();
  constructSky();
  constructClouds();
  constructTrees();
  constructFakeSoil();
  constructSoil();
  auto mines = constructMines();
  constructNumericTiles(mines);
  constructGradient();
  constructCoins();
  constructGoldNuggets(mines);
  constructWanderers();
  constructSticks();
  constructExit();
  constructCoinLabel();
  constructCoinCounter();
  constructTimeLabel();
  constructTimeCounter();
  scene.throwingModeIndicator = constructThrowingModeIndicator();

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

EntityId SceneBuilderImpl::constructPlayer()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
  >();

  sysGrid.addEntity(id, 0, 0);

  Vec2f size{ 0.0625f, 0.0625f };
  Vec2f pos{ 0.f, 0.f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = Rectf{
      .x = pxToUvX(384.f),
      .y = pxToUvY(256.f, 48.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(48.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Player)
  };

  sysRender.addEntity(id, render);

  long animationDuration = 16;
  float_t delta = 0.015625f;

  auto makeFrame = [](float_t x, float_t y, float_t tx, float_t ty, const Vec4f& col) {
    return AnimationFrame{
      .pos = Vec2f{ x, y },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty, 48.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(48.f)
      },
      .colour = col
    };
  };

  const Vec4f white{ 1.f, 1.f, 1.f, 1.f };

  auto animMoveLeft = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_left"),
    .duration = animationDuration,
    .frames = {
      makeFrame(-delta * 1.f, 0.f, 384.f, 352.f, white),
      makeFrame(-delta * 2.f, 0.f, 416.f, 352.f, white),
      makeFrame(-delta * 3.f, 0.f, 448.f, 352.f, white),
      makeFrame(-delta * 4.f, 0.f, 480.f, 352.f, white)
    }
  });

  auto animMoveRight = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_right"),
    .duration = animationDuration,
    .frames = {
      makeFrame(delta * 1.f, 0.f, 384.f, 304.f, white),
      makeFrame(delta * 2.f, 0.f, 416.f, 304.f, white),
      makeFrame(delta * 3.f, 0.f, 448.f, 304.f, white),
      makeFrame(delta * 4.f, 0.f, 480.f, 304.f, white)
    }
  });

  auto animMoveUp = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_up"),
    .duration = animationDuration,
    .frames = {
      makeFrame(0.f, delta * 1.f, 384.f, 256.f, white),
      makeFrame(0.f, delta * 2.f, 416.f, 256.f, white),
      makeFrame(0.f, delta * 3.f, 448.f, 256.f, white),
      makeFrame(0.f, delta * 4.f, 480.f, 256.f, white)
    }
  });

  auto animMoveDown = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_down"),
    .duration = animationDuration,
    .frames = {
      makeFrame(0.f, -delta * 1.f, 384.f, 400.f, white),
      makeFrame(0.f, -delta * 2.f, 416.f, 400.f, white),
      makeFrame(0.f, -delta * 3.f, 448.f, 400.f, white),
      makeFrame(0.f, -delta * 4.f, 480.f, 400.f, white)
    }
  });

  auto animDie = std::unique_ptr<Animation>(new Animation{
    .name = hashString("die"),
    .duration = 24,
    .frames = {
      makeFrame(0.f, 0.f, 384.f, 400.f, { 1.f, 1.f, 1.f, 1.f }),
      makeFrame(0.f, 0.f, 384.f, 400.f, { 0.f, 1.f, 0.f, 0.f })
    }
  });

  sysAnimation.addEntity(id, AnimationData{
    .animations = {
      sysAnimation.addAnimation(std::move(animMoveLeft)),
      sysAnimation.addAnimation(std::move(animMoveRight)),
      sysAnimation.addAnimation(std::move(animMoveUp)),
      sysAnimation.addAnimation(std::move(animMoveDown)),
      sysAnimation.addAnimation(std::move(animDie))
    }
  });

  auto behaviour = createBPlayer(m_ecs, m_eventSystem, id);
  sysBehaviour.addBehaviour(id, std::move(behaviour));

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
    .zIndex = static_cast<uint32_t>(ZIndex::Sky)
  };

  sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructClouds()
{
  static auto strIdle = hashString("idle");

  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  long animationDuration = 15000;

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = hashString("idle"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ -2.f * GRID_W * GRID_CELL_W, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
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
      .zIndex = static_cast<uint32_t>(ZIndex::Cloud),
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
      .zIndex = static_cast<uint32_t>(ZIndex::Cloud),
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
      .zIndex = static_cast<uint32_t>(ZIndex::Trees)
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
      .zIndex = static_cast<uint32_t>(ZIndex::Soil),
      .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f },
      .text = ""
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

  auto makeFrame = [](float_t tx, float_t ty, float_t a) {
    return AnimationFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .colour = Vec4f{ 1.f, 1.f, 1.f, a }
    };
  };

  auto animCollect = std::unique_ptr<Animation>(new Animation{
    .name = hashString("collect"),
    .duration = 15,
    .frames = {
      makeFrame(384.f, 0.f, 1.0),
      makeFrame(384.f, 0.f, 0.f)
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
        .zIndex = static_cast<uint32_t>(ZIndex::Soil),
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f },
        .text = ""
      };

      sysRender.addEntity(id, render);

      sysAnimation.addEntity(id, AnimationData{
        .animations = { animCollectId }
      });

      auto collectBehaviour = createBCollectable(m_ecs, m_eventSystem, id, m_playerId, 0);
      sysBehaviour.addBehaviour(id, std::move(collectBehaviour));

      auto dissolveBehaviour = createBGeneric(hashString("dissolve"),
        { g_strEntityExplode, g_strAnimationFinish }, [&sysAnimation, this, id](const Event& e) {

        if (e.name == g_strEntityExplode) {
          if (sysAnimation.hasAnimationPlaying(id)) {
            sysAnimation.finishAnimation(id);
          }
          sysAnimation.playAnimation(id, hashString("collect"));
        }
        else if (e.name == g_strAnimationFinish) {
          auto& event = dynamic_cast<const EAnimationFinish&>(e);
          if (event.entityId == id && event.animationName == hashString("collect")) {
            m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(id));
          }
        }
      });

      sysBehaviour.addBehaviour(id, std::move(dissolveBehaviour));
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

  auto makeFrame = [](float_t tx, float_t ty) {
    return AnimationFrame{
      .pos = Vec2f{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
      .scale = Vec2f{ 2.f, 2.f },
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty, 64.f),
        .w = pxToUvW(64.f),
        .h = pxToUvH(64.f)
      },
      .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
    };
  };

  auto animExplode = std::unique_ptr<Animation>(new Animation{
    .name = hashString("explode"),
    .duration = 30,
    .startPos{ -GRID_CELL_W * 0.5f, -GRID_CELL_H * 0.5f },
    .frames = {
      makeFrame(448.f, 0.f),
      makeFrame(512.f, 0.f),
      makeFrame(576.f, 0.f),
      makeFrame(640.f, 0.f),
      makeFrame(448.f, 64.f),
      makeFrame(512.f, 64.f),
      makeFrame(576.f, 64.f),
      makeFrame(640.f, 64.f),
      makeFrame(448.f, 128.f),
      makeFrame(512.f, 128.f),
      makeFrame(576.f, 128.f),
      makeFrame(640.f, 128.f),
      makeFrame(448.f, 192.f),
      makeFrame(512.f, 192.f),
      makeFrame(576.f, 192.f)
    }
  });

  auto animExplodeId = sysAnimation.addAnimation(std::move(animExplode));

  std::vector<Vec2i> coords = randomGridCoords();

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
      .zIndex = static_cast<uint32_t>(ZIndex::Mine)
    };

    sysRender.addEntity(id, render);

    auto onEvent = [this, id, x, y, &sysGrid, &sysAnimation, &sysRender](const Event& e) {
      if (e.name == g_strEntityLandOn) {
        auto& event = dynamic_cast<const EEntityLandOn&>(e);

        if (event.pos == Vec2i{ x, y }) {
          EntityIdSet targets = sysGrid.getEntities(x - 1, y - 1, x + 1, y + 1);
          sysGrid.removeEntity(id);
          m_eventSystem.queueEvent(std::make_unique<EEntityExplode>(id, event.pos, targets));
        }
      }
      else if (e.name == g_strEntityExplode) {
        auto& event = dynamic_cast<const EEntityExplode&>(e);

        if (event.entityId == id) {
          sysAnimation.playAnimation(id, strExplode);
          sysRender.setZIndex(id, static_cast<uint32_t>(ZIndex::Explosion));
        }
      }
      else if (e.name == g_strAnimationFinish) {
        auto& event = dynamic_cast<const EAnimationFinish&>(e);

        if (event.entityId == id && event.animationName == strExplode) {
          m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(id));
        }
      }
    };

    auto behaviour = createBGeneric(strExplode, { g_strEntityLandOn, g_strEntityExplode }, onEvent);
    sysBehaviour.addBehaviour(id, std::move(behaviour));

    sysAnimation.addEntity(id, AnimationData{
      .animations = { animExplodeId }
    });

    mines.insert({ x, y });
  }

  return mines;
}

void SceneBuilderImpl::constructNumericTiles(const std::set<std::pair<int, int>>& mines)
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
        .zIndex = static_cast<uint32_t>(ZIndex::NumericTile)
      };

      sysRender.addEntity(id, render);
      sysRender.setVisible(id, !mines.contains({ i, j }));

      auto behaviour = createBNumericTile(m_ecs, m_eventSystem, m_timeService, id, Vec2i{ i, j },
        value);

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

  Vec2f size{ GRID_CELL_W * GRID_W, GRID_CELL_H * (GRID_H + 1) };
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
    .zIndex = static_cast<uint32_t>(ZIndex::Gradient)
  };

  sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructCoins()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  size_t numCoins = 10; // TODO

  auto makeFrame = [](float_t tx, float_t ty, float_t a) {
    return AnimationFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .colour = Vec4f{ 1.f, 1.f, 1.f, a }
    };
  };

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = hashString("idle"),
    .duration = 60,
    .frames = {
      makeFrame(960.f, 0.f, 1.f),
      makeFrame(976.f, 0.f, 1.f),
      makeFrame(992.f, 0.f, 1.f),
      makeFrame(1008.f, 0.f, 1.f),
      makeFrame(960.f, 16.f, 1.f),
      makeFrame(976.f, 16.f, 1.f),
      makeFrame(992.f, 16.f, 1.f),
      makeFrame(1008.f, 16.f, 1.f),
      makeFrame(960.f, 32.f, 1.f),
      makeFrame(976.f, 32.f, 1.f),
      makeFrame(992.f, 32.f, 1.f),
      makeFrame(1008.f, 32.f, 1.f),
      makeFrame(960.f, 48.f, 1.f),
      makeFrame(976.f, 48.f, 1.f),
      makeFrame(992.f, 48.f, 1.f),
      makeFrame(1008.f, 48.f, 1.f)
    }
  });

  auto animIdleId = sysAnimation.addAnimation(std::move(animIdle));

  auto animCollect = std::unique_ptr<Animation>(new Animation{
    .name = hashString("collect"),
    .duration = 12,
    .frames = {
      makeFrame(960.f, 0.f, 1.f),
      makeFrame(976.f, 0.f, 0.9f),
      makeFrame(992.f, 0.f, 0.8f),
      makeFrame(1008.f, 0.f, 0.7f),
      makeFrame(960.f, 16.f, 0.6f),
      makeFrame(976.f, 16.f, 0.5f),
      makeFrame(992.f, 16.f, 0.4f),
      makeFrame(1008.f, 16.f, 0.3f),
      makeFrame(960.f, 32.f, 0.2f),
      makeFrame(976.f, 32.f, 0.1f),
      makeFrame(992.f, 32.f, 0.f)
    }
  });

  auto animCollectId = sysAnimation.addAnimation(std::move(animCollect));

  std::vector<Vec2i> coords = randomGridCoords();

  for (size_t i = 0; i < numCoins; ++i) {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();

    m_entities.insert(id);

    int x = coords[i][0];
    int y = coords[i][1];

    sysGrid.addEntity(id, x, y);

    Vec2f size{ 0.03f, 0.03f };
    Vec2f offset{ (GRID_CELL_W - size[0]) * 0.5f, (GRID_CELL_H - size[1]) * 0.5f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    SpatialData spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
        .x = pxToUvX(960.f),
        .y = pxToUvY(0.f, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Coin)
    };

    sysRender.addEntity(id, render);

    sysAnimation.addEntity(id, AnimationData{
      .animations = { animIdleId, animCollectId }
    });

    sysAnimation.playAnimation(id, hashString("idle"), true);
    sysAnimation.seek(id, randomInt());

    auto behaviour = createBCollectable(m_ecs, m_eventSystem, id, m_playerId, 1);
    sysBehaviour.addBehaviour(id, std::move(behaviour));
  }
}

void SceneBuilderImpl::constructGoldNuggets(const std::set<std::pair<int, int>>& mines)
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  auto makeFrame = [](float_t tx, float_t ty, float_t a) {
    return AnimationFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty, 32.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(32.f)
      },
      .colour = Vec4f{ 1.f, 1.f, 1.f, a }
    };
  };

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = hashString("idle"),
    .duration = 120,
    .frames = {
      makeFrame(992, 64, 1.f),
      makeFrame(960, 96, 1.f),
      makeFrame(992, 96, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 1.f)
    }
  });

  auto animIdleId = sysAnimation.addAnimation(std::move(animIdle));

  auto animCollect = std::unique_ptr<Animation>(new Animation{
    .name = hashString("collect"),
    .duration = 12,
    .frames = {
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 0.f)
    }
  });

  auto animCollectId = sysAnimation.addAnimation(std::move(animCollect));

  auto coords = randomGridCoords();
  size_t numNuggets = 3; // TODO

  size_t nuggetsConstructed = 0;
  for (size_t i = 0; i < coords.size(); ++i) {
    int x = coords[i][0];
    int y = coords[i][1];

    if (mines.contains({ x, y })) {
      continue;
    }

    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();

    m_entities.insert(id);

    sysGrid.addEntity(id, x, y);

    Vec2f size{ 0.03f, 0.03f };
    Vec2f offset{ (GRID_CELL_W - size[0]) * 0.5f, (GRID_CELL_H - size[1]) * 0.5f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    SpatialData spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
        .x = pxToUvX(960.f),
        .y = pxToUvY(64.f, 32.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Nugget)
    };

    sysRender.addEntity(id, render);

    sysAnimation.addEntity(id, AnimationData{
      .animations = { animIdleId, animCollectId }
    });

    sysAnimation.playAnimation(id, hashString("idle"), true);
    sysAnimation.seek(id, randomInt());

    auto behaviour = createBCollectable(m_ecs, m_eventSystem, id, m_playerId, 5);
    sysBehaviour.addBehaviour(id, std::move(behaviour));

    ++nuggetsConstructed;

    if (nuggetsConstructed >= numNuggets) {
      break;
    }
  }
}

void SceneBuilderImpl::constructWanderers()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  size_t numWanderers = 3; // TODO

  auto makeFrame = [](float_t x, float_t y, float_t tx, float_t ty, float_t a) {
    return AnimationFrame{
      .pos = Vec2f{ x, y },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty, 48.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(48.f)
      },
      .colour = Vec4f{ 1.f, 1.f, 1.f, a }
    };
  };

  auto animFadeIn = std::unique_ptr<Animation>(new Animation{
    .name = hashString("fade_in"),
    .duration = 30,
    .frames = {
      makeFrame(0.f, 0.f, 256.f, 400.f, 0.f),
      makeFrame(0.f, 0.f, 256.f, 400.f, 1.f)
    }
  });

  auto animFadeInId = sysAnimation.addAnimation(std::move(animFadeIn));

  long animationDuration = 32;
  float_t delta = 0.015625f;

  auto animMoveLeft = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_left"),
    .duration = animationDuration,
    .frames = {
      makeFrame(-delta * 1.f, 0.f, 256.f, 352.f, 1.f),
      makeFrame(-delta * 2.f, 0.f, 288.f, 352.f, 1.f),
      makeFrame(-delta * 3.f, 0.f, 320.f, 352.f, 1.f),
      makeFrame(-delta * 4.f, 0.f, 352.f, 352.f, 1.f)
    }
  });

  auto animMoveLeftId = sysAnimation.addAnimation(std::move(animMoveLeft));

  auto animMoveRight = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_right"),
    .duration = animationDuration,
    .frames = {
      makeFrame(delta * 1.f, 0.f, 256.f, 304.f, 1.f),
      makeFrame(delta * 2.f, 0.f, 288.f, 304.f, 1.f),
      makeFrame(delta * 3.f, 0.f, 320.f, 304.f, 1.f),
      makeFrame(delta * 4.f, 0.f, 352.f, 304.f, 1.f)
    }
  });

  auto animMoveRightId = sysAnimation.addAnimation(std::move(animMoveRight));

  auto animMoveUp = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_up"),
    .duration = animationDuration,
    .frames = {
      makeFrame(0.f, delta * 1.f, 256.f, 256.f, 1.f),
      makeFrame(0.f, delta * 2.f, 288.f, 256.f, 1.f),
      makeFrame(0.f, delta * 3.f, 320.f, 256.f, 1.f),
      makeFrame(0.f, delta * 4.f, 352.f, 256.f, 1.f)
    }
  });

  auto animMoveUpId = sysAnimation.addAnimation(std::move(animMoveUp));

  auto animMoveDown = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_down"),
    .duration = animationDuration,
    .frames = {
      makeFrame(0.f, -delta * 1.f, 256.f, 400.f, 1.f),
      makeFrame(0.f, -delta * 2.f, 288.f, 400.f, 1.f),
      makeFrame(0.f, -delta * 3.f, 320.f, 400.f, 1.f),
      makeFrame(0.f, -delta * 4.f, 352.f, 400.f, 1.f)
    }
  });

  auto animMoveDownId = sysAnimation.addAnimation(std::move(animMoveDown));

  auto coords = randomGridCoords();

  for (size_t i = 0; i < numWanderers; ++i) {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();

    m_entities.insert(id);

    int x = coords[i][0];
    int y = coords[i][1];

    sysGrid.addEntity(id, x, y);

    Vec2f size{ 0.0625, 0.0625f };
    Vec2f offset{ (GRID_CELL_W - size[0]) * 0.5f, (GRID_CELL_H - size[1]) * 0.5f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    SpatialData spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
      .x = pxToUvX(256.f),
      .y = pxToUvY(256.f, 48.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(48.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Wanderer),
      .colour = Vec4f{ 1.f, 1.f, 1.f, 0.f }
    };

    sysRender.addEntity(id, render);
    sysRender.setVisible(id, false);

    sysAnimation.addEntity(id, AnimationData{
      .animations = {
        animFadeInId,
        animMoveLeftId,
        animMoveRightId,
        animMoveUpId,
        animMoveDownId
      }
    });

    auto behaviour = createBWanderer(m_ecs, m_eventSystem, m_timeService, id, m_playerId);
    sysBehaviour.addBehaviour(id, std::move(behaviour));
  }
}

void SceneBuilderImpl::constructSticks()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  size_t numSticks = 4; // TODO

  auto animThrow = std::unique_ptr<Animation>(new Animation{
    .name = hashString("throw"),
    .duration = 30,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
      }
    }
  });

  auto animThrowId = sysAnimation.addAnimation(std::move(animThrow));

  auto animFade = std::unique_ptr<Animation>(new Animation{
    .name = hashString("fade"),
    .duration = 30,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.f }
      }
    }
  });

  auto animFadeId = sysAnimation.addAnimation(std::move(animFade));

  auto coords = randomGridCoords();

  for (size_t i = 0; i < numSticks; ++i) {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
    >();

    m_entities.insert(id);

    int x = coords[i][0];
    int y = coords[i][1];

    sysGrid.addEntity(id, x, y);

    Vec2f size{ GRID_CELL_W, GRID_CELL_H };
    Vec2f offset{ 0.5f * GRID_CELL_W, 0.f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    SpatialData spatial{
      .transform = spriteTransform(pos, size, PIf / 4.f, { 0.f, 0.5f }),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    SpriteData render{
      .textureRect = Rectf{
        .x = pxToUvX(960.f),
        .y = pxToUvY(160.f, 32.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Stick)
    };

    sysRender.addEntity(id, render);

    sysAnimation.addEntity(id, AnimationData{
      .animations = { animThrowId, animFadeId }
    });

    auto behaviour = createBStick(m_ecs, m_eventSystem, id, m_playerId, animThrowId);
    sysBehaviour.addBehaviour(id, std::move(behaviour));
  }
}

void SceneBuilderImpl::constructExit()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  auto makeFrame = [](float_t tx, float_t ty) {
    return AnimationFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
    };
  };

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = hashString("idle"),
    .duration = 32,
    .frames = {
      makeFrame(128.f, 432.f),
      makeFrame(144.f, 432.f),
      makeFrame(160.f, 432.f),
      makeFrame(176.f, 432.f),
      makeFrame(192.f, 432.f),
      makeFrame(208.f, 432.f),
      makeFrame(224.f, 432.f),
      makeFrame(240.f, 432.f)
    }
  });

  auto animIdleId = sysAnimation.addAnimation(std::move(animIdle));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
  >();

  m_entities.insert(id);

  Vec2f size{ 0.0625f, 0.0625f };
  Vec2f pos{ GRID_CELL_W * (GRID_W - 1), GRID_CELL_H * (GRID_H - 1) };

  sysGrid.addEntity(id, (GRID_W - 1), (GRID_H - 1));

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = Rectf{
      .x = pxToUvX(128.f),
      .y = pxToUvY(416.f, 16.f),
      .w = pxToUvW(16.f),
      .h = pxToUvH(16.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Exit)
  };

  sysRender.addEntity(id, render);

  sysAnimation.addEntity(id, AnimationData{ .animations = { animIdleId } });

  auto behaviour = createBGeneric(hashString("exit"), { g_strGoldTargetAttained },
    [&sysAnimation, id](const Event& e) {

    if (e.name == g_strGoldTargetAttained) {
      sysAnimation.playAnimation(id, hashString("idle"), true);
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));
}

void SceneBuilderImpl::constructTimeLabel()
{
  constructStaticEntity({ 0.01f, 0.94f }, { 0.05f, 0.05f }, Rectf{
    .x = pxToUvX(960.f),
    .y = pxToUvY(128.f, 32.f),
    .w = pxToUvW(24.f),
    .h = pxToUvH(32.f)
  }, static_cast<uint32_t>(ZIndex::Hud));
}

void SceneBuilderImpl::constructTimeCounter()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite, CDynamicText
  >();

  m_entities.insert(id);

  Vec2f size{ 0.035f, 0.035f };
  Vec2f pos{ 0.07f, 0.94f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Hud),
    .colour = Vec4f{ 0.f, 1.f, 0.f, 1.f },
    .text = "100", // TODO
    .isDynamicText = true
  };

  sysRender.addEntity(id, render);

  auto behaviour = createBGeneric(hashString("on_tick"), { g_strTimerTick },
    [&, id](const Event& e) {

    if (e.name == g_strTimerTick) {
      auto& event = dynamic_cast<const ETimerTick&>(e);

      std::stringstream ss;
      ss << std::setw(3) << std::setfill('0') << event.timeRemaining;

      // TODO: Write helper function for this?

      char* buffer = m_ecs.componentStore().component<CDynamicText>(id).text;

      memset(buffer, ' ', DYNAMIC_TEXT_MAX_LEN);
      strncpy(buffer, ss.str().data(), 3);

      if (event.timeRemaining <= 10) {
        m_ecs.componentStore().component<CSprite>(id).colour = { 1.f, 0.f, 0.f, 1.f };
      }
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));
}

void SceneBuilderImpl::constructCoinLabel()
{
  constructStaticEntity({ 0.22f, 0.94f }, { 0.05f, 0.05f }, Rectf{
    .x = pxToUvX(984.f),
    .y = pxToUvY(128.f, 32.f),
    .w = pxToUvW(24.f),
    .h = pxToUvH(32.f)
  }, static_cast<uint32_t>(ZIndex::Hud));
}

void SceneBuilderImpl::constructCoinCounter()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite, CDynamicText
  >();

  m_entities.insert(id);

  Vec2f size{ 0.035f, 0.035f };
  Vec2f pos{ 0.27f, 0.94f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  int coinsRequired = 12; // TODO

  SpriteData render{
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Hud),
    .colour = Vec4f{ 0.f, 1.f, 0.f, 1.f },
    .text = "12",  // TODO
    .isDynamicText = true
  };

  sysRender.addEntity(id, render);

  auto behaviour = createBCoinCounter(coinsRequired, m_ecs, m_eventSystem, id);
  sysBehaviour.addBehaviour(id, std::move(behaviour));
}

EntityId SceneBuilderImpl::constructStaticEntity(const Vec2f& pos, const Vec2f& size,
  const Rectf& texRect, uint32_t zIndex)
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
  >();

  m_entities.insert(id);

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = texRect,
    .zIndex = zIndex
  };

  sysRender.addEntity(id, render);

  return id;
}

EntityId SceneBuilderImpl::constructThrowingModeIndicator()
{
  auto id = constructStaticEntity({ 1.f, 0.94f }, { 0.05f, 0.05f }, Rectf{
    .x = pxToUvX(992.f),
    .y = pxToUvY(160.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  }, static_cast<uint32_t>(ZIndex::Hud));

  m_ecs.componentStore().component<CSprite>(id).visible = false;

  return id;
}

} // namespace

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, Ecs& ecs, TimeService& timeService)
{
  return std::make_unique<SceneBuilderImpl>(eventSystem, ecs, timeService);  
}
