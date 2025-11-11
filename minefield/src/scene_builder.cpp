#include "scene_builder.hpp"
#include "game_options.hpp"
#include "units.hpp"
#include "sys_grid.hpp"
#include "b_player.hpp"
#include "b_collectable.hpp"
#include "b_numeric_tile.hpp"
#include "b_wanderer.hpp"
#include "b_coin_counter.hpp"
#include "b_stick.hpp"
#include "b_exit.hpp"
#include "game_events.hpp"
#include <fge/b_generic.hpp>
#include <fge/sys_animation_2d.hpp>
#include <fge/sys_behaviour.hpp>
#include <fge/sys_render_2d.hpp>
#include <fge/sys_spatial.hpp>
#include <fge/ecs.hpp>
#include <fge/systems.hpp>
#include <fge/events.hpp>
#include <random>
#include <cstring>

using fge::EntityId;
using fge::EntityIdSet;
using fge::HashedString;
using fge::hashString;
using fge::Event;
using fge::EventSystem;
using fge::Ecs;
using fge::Vec2i;
using fge::Vec2f;
using fge::Vec3f;
using fge::Vec4f;
using fge::Rectf;
using fge::Mat4x4f;
using fge::identityMatrix;
using fge::SysAnimation2d;
using fge::SysRender2d;
using fge::SysSpatial;
using fge::SysBehaviour;
using fge::Animation2dId;
using fge::Animation2d;
using fge::Animation2dFrame;
using fge::DSprite;
using fge::DSpatial;
using fge::DDynamicText;
using fge::DQuad;
using fge::DText;
using fge::DAnimation2d;
using fge::CLocalTransform;
using fge::CGlobalTransform;
using fge::CSpatialFlags;
using fge::CQuad;
using fge::CDynamicText;
using fge::CRender2d;
using fge::CSprite;
using fge::pxToUvX;
using fge::pxToUvY;
using fge::pxToUvW;
using fge::pxToUvH;

namespace
{

enum class ZIndex : uint32_t
{
  Background,
  NumericTile,
  Mine,
  Nugget,
  Soil,
  Sky,
  Trees,
  Exit,
  Cloud,
  Gradient,
  Stick,
  Coin,
  Player,
  Wanderer,
  Explosion,
  Hud
};

// TODO: Move this
Mat4x4f spriteTransform(const Vec2f& pos, const Vec2f& size, float rotation = 0.f,
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

class SceneBuilderImpl : public SceneBuilder
{
  public:
    SceneBuilderImpl(EventSystem& eventSystem, Ecs& ecs, const GameOptionsManager& options);

    Scene buildScene(uint32_t level) override;
    EntityIdSet entities() const override;

  private:
    EventSystem& m_eventSystem;
    Ecs& m_ecs;
    const GameOptionsManager& m_options;
    EntityIdSet m_entities;
    EntityId m_worldRoot;
    EntityId m_playerId;

    EntityId constructStaticEntity(const Vec2f& pos, const Vec2f& size, const Rectf& texRect,
      uint32_t zIndex);
    EntityId constructWorldRoot();
    EntityId constructPlayer();
    void constructBackQuad();
    void constructSky();
    void constructClouds();
    void constructTrees();
    void constructFakeSoil();
    void constructSoil();
    std::set<std::pair<int, int>> constructMines(uint32_t numMines);
    void constructNumericTiles(const std::set<std::pair<int, int>>& mines);
    void constructGradient();
    void constructCoins(uint32_t numCoins);
    void constructGoldNuggets(uint32_t numNuggets, const std::set<std::pair<int, int>>& mines);
    void constructWanderers(uint32_t numWanderers, const std::set<std::pair<int, int>>& mines);
    void constructSticks(uint32_t numSticks, const std::set<std::pair<int, int>>& mines);
    void constructExit();
    void constructCoinLabel();
    void constructCoinCounter(uint32_t goldRequired);
    void constructTimeLabel();
    void constructTimeCounter(uint32_t timeAvailable);
    EntityId constructThrowingModeIndicator();
    EntityId constructRestartGamePrompt();
};

SceneBuilderImpl::SceneBuilderImpl(EventSystem& eventSystem, Ecs& ecs,
  const GameOptionsManager& options)
  : m_eventSystem(eventSystem)
  , m_ecs(ecs)
  , m_options(options)
{
}

EntityIdSet SceneBuilderImpl::entities() const
{
  return m_entities;
}

Scene SceneBuilderImpl::buildScene(uint32_t level)
{
  m_entities.clear();

  Scene scene;

  auto options = m_options.getOptionsForLevel(level);

  m_worldRoot = constructWorldRoot();
  m_playerId = constructPlayer();
  constructBackQuad();
  constructSky();
  constructClouds();
  constructTrees();
  constructFakeSoil();
  constructSoil();
  auto mines = constructMines(options.mines);
  constructNumericTiles(mines);
  constructGradient();
  constructCoins(options.coins);
  constructGoldNuggets(options.nuggets, mines);
  constructWanderers(options.wanderers, mines);
  constructSticks(options.sticks, mines);
  constructExit();
  constructCoinLabel();
  constructCoinCounter(options.goldRequired);
  constructTimeLabel();
  constructTimeCounter(options.timeAvailable);
  scene.throwingModeIndicator = constructThrowingModeIndicator();
  scene.restartGamePrompt = constructRestartGamePrompt();

  scene.player = m_playerId;
  scene.worldRoot = m_worldRoot;

  return scene;
}

EntityId SceneBuilderImpl::constructWorldRoot()
{
  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  //m_entities.insert(id);

  auto& sysSpatial = m_ecs.system<SysSpatial>();

  sysSpatial.addEntity(id, DSpatial{
    .transform = identityMatrix<float, 4>(),
    .parent = sysSpatial.root(),
    .enabled = true
  });

  return id;
}

EntityId SceneBuilderImpl::constructPlayer()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
  >();

  m_entities.insert(id);

  sysGrid.addEntity(id, 0, 0);

  Vec2f size{ 0.0625f, 0.0625f };
  Vec2f pos{ 0.f, 0.f };

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DSprite render{
    .scissor = MAIN_SCISSOR,
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
  float delta = 0.015625f;

  auto makeFrame = [](const Vec2f& pos, float tx, float ty, const Vec4f& col,
    float scale = 1.f) {

    return Animation2dFrame{
      .pos = pos,
      .scale = { scale, scale },
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

  auto animMoveLeft = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("move_left"),
    .duration = animationDuration,
    .frames = {
      makeFrame({ -delta * 1.f, 0.f }, 384.f, 352.f, white),
      makeFrame({ -delta * 2.f, 0.f }, 416.f, 352.f, white),
      makeFrame({ -delta * 3.f, 0.f }, 448.f, 352.f, white),
      makeFrame({ -delta * 4.f, 0.f }, 480.f, 352.f, white)
    }
  });

  auto animMoveRight = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("move_right"),
    .duration = animationDuration,
    .frames = {
      makeFrame({ delta * 1.f, 0.f }, 384.f, 304.f, white),
      makeFrame({ delta * 2.f, 0.f }, 416.f, 304.f, white),
      makeFrame({ delta * 3.f, 0.f }, 448.f, 304.f, white),
      makeFrame({ delta * 4.f, 0.f }, 480.f, 304.f, white)
    }
  });

  auto animMoveUp = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("move_up"),
    .duration = animationDuration,
    .frames = {
      makeFrame({ 0.f, delta * 1.f }, 384.f, 256.f, white),
      makeFrame({ 0.f, delta * 2.f }, 416.f, 256.f, white),
      makeFrame({ 0.f, delta * 3.f }, 448.f, 256.f, white),
      makeFrame({ 0.f, delta * 4.f }, 480.f, 256.f, white)
    }
  });

  auto animMoveDown = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("move_down"),
    .duration = animationDuration,
    .frames = {
      makeFrame({ 0.f, -delta * 1.f }, 384.f, 400.f, white),
      makeFrame({ 0.f, -delta * 2.f }, 416.f, 400.f, white),
      makeFrame({ 0.f, -delta * 3.f }, 448.f, 400.f, white),
      makeFrame({ 0.f, -delta * 4.f }, 480.f, 400.f, white)
    }
  });

  auto animDie = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("die"),
    .duration = 24,
    .frames = {
      makeFrame({ 0.f, 0.f }, 384.f, 400.f, { 1.f, 1.f, 1.f, 1.f }),
      makeFrame({ 0.f, 0.f }, 384.f, 400.f, { 0.f, 1.f, 0.f, 0.f })
    }
  });

  auto calcOffset = [](float scale) {
    float w = GRID_CELL_W;
    float h = GRID_CELL_H;
    float w_ = GRID_CELL_W * scale;
    float h_ = GRID_CELL_H * scale;

    return Vec2f{ w - w_, h - h_ } * 0.5f;
  };

  auto animEnterPortal = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("enter_portal"),
    .duration = 30,
    .frames = {
      makeFrame(calcOffset(0.9f), 384.f, 256.f, white, 0.9f),
      makeFrame(calcOffset(0.8f), 384.f, 256.f, white, 0.8f),
      makeFrame(calcOffset(0.7f), 384.f, 256.f, white, 0.7f),
      makeFrame(calcOffset(0.6f), 384.f, 256.f, white, 0.6f),
      makeFrame(calcOffset(0.5f), 384.f, 256.f, white, 0.5f),
      makeFrame(calcOffset(0.4f), 384.f, 256.f, white, 0.4f),
      makeFrame(calcOffset(0.3f), 384.f, 256.f, white, 0.3f),
      makeFrame(calcOffset(0.2f), 384.f, 256.f, white, 0.2f),
      makeFrame(calcOffset(0.1f), 384.f, 256.f, white, 0.1f),
      makeFrame(calcOffset(0.1f), 384.f, 256.f, Vec4f{}, 0.1f),
      makeFrame(calcOffset(0.1f), 384.f, 256.f, Vec4f{}, 0.1f),
      makeFrame(calcOffset(0.1f), 384.f, 256.f, Vec4f{}, 0.1f),
      makeFrame(calcOffset(0.1f), 384.f, 256.f, Vec4f{}, 0.1f)
    }
  });

  sysAnimation.addEntity(id, DAnimation2d{
    .animations = {
      sysAnimation.addAnimation(std::move(animMoveLeft)),
      sysAnimation.addAnimation(std::move(animMoveRight)),
      sysAnimation.addAnimation(std::move(animMoveUp)),
      sysAnimation.addAnimation(std::move(animMoveDown)),
      sysAnimation.addAnimation(std::move(animDie)),
      sysAnimation.addAnimation(std::move(animEnterPortal))
    }
  });

  auto behaviour = createBPlayer(m_ecs, m_eventSystem, id);
  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return id;
}

void SceneBuilderImpl::constructSky()
{
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysSpatial = m_ecs.system<SysSpatial>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
  >();

  m_entities.insert(id);

  Vec2f size{ GRID_CELL_W * GRID_W, 4.5f * GRID_CELL_H };
  Vec2f pos{ 0.f, 11.5f * GRID_CELL_H };

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DSprite render{
    .scissor = MAIN_SCISSOR,
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

  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  const Vec4f colour{ 1.f, 0.8f, 0.5f, 0.6f };
  long animationDuration = 18000;

  auto animIdle = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("idle"),
    .duration = animationDuration,
    .frames = {
      Animation2dFrame{
        .pos = Vec2f{ -2.f * GRID_W * GRID_CELL_W, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = colour
      }
    }
  });

  auto animIdleId = sysAnimation.addAnimation(std::move(animIdle));

  {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();

    m_entities.insert(id);

    Vec2f size{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H };
    Vec2f pos{ GRID_CELL_W * GRID_W, 0.8f };

    DSpatial spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
      .textureRect = Rectf{
        .x = pxToUvX(256.f),
        .y = pxToUvY(0.f, 32.f),
        .w = pxToUvW(128.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Cloud),
      .colour = colour
    };

    sysRender.addEntity(id, render);

    DAnimation2d animation{
      .animations = { animIdleId }
    };

    sysAnimation.addEntity(id, animation);

    sysAnimation.playAnimation(id, strIdle, true);
    sysAnimation.seek(id, animationDuration / 2);
  }

  {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();

    m_entities.insert(id);

    Vec2f size{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H };
    Vec2f pos{ GRID_CELL_W * GRID_W, 0.8f };

    DSpatial spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
      .textureRect = Rectf{
        .x = pxToUvX(256.f),
        .y = pxToUvY(32.f, 32.f),
        .w = pxToUvW(128.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Cloud),
      .colour = colour
    };

    sysRender.addEntity(id, render);

    DAnimation2d animation{
      .animations = { animIdleId }
    };

    sysAnimation.addEntity(id, animation);

    sysAnimation.playAnimation(id, strIdle, true);
  }
}

void SceneBuilderImpl::constructTrees()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
  >();
  
  m_entities.insert(id);

  Vec2f size{ GRID_CELL_W * GRID_W, 3.f * GRID_CELL_H };
  Vec2f pos{ 0.f, 11.f * GRID_CELL_H };

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);
  
  DSprite render{
    .scissor = MAIN_SCISSOR,
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
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();

  for (size_t i = 0; i < GRID_W; ++i) {
    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();
    
    m_entities.insert(id);

    float x = GRID_CELL_W * i;

    Vec2f size{ GRID_CELL_W, GRID_CELL_H };
    Vec2f pos{ x, 11.f * GRID_CELL_H };

    DSpatial spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
      .textureRect = Rectf{
        .x = pxToUvX(384.f),
        .y = pxToUvY(0.f, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Soil),
      .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
    };

    sysRender.addEntity(id, render);
  }
}

void SceneBuilderImpl::constructSoil()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();

  auto makeFrame = [](float tx, float ty, float a) {
    return Animation2dFrame{
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

  auto animCollect = std::unique_ptr<Animation2d>(new Animation2d{
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
        CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
      >();
      
      m_entities.insert(id);

      sysGrid.addEntity(id, i, j);

      float x = GRID_CELL_W * i;
      float y = GRID_CELL_H * j;

      Vec2f size{ GRID_CELL_W, GRID_CELL_H };
      Vec2f pos{ x, y };

      DSpatial spatial{
        .transform = spriteTransform(pos, size),
        .parent = m_worldRoot,
        .enabled = true
      };

      sysSpatial.addEntity(id, spatial);

      DSprite render{
        .scissor = MAIN_SCISSOR,
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .zIndex = static_cast<uint32_t>(ZIndex::Soil),
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
      };

      sysRender.addEntity(id, render);

      sysAnimation.addEntity(id, DAnimation2d{
        .animations = { animCollectId }
      });

      auto collectBehaviour = createBCollectable(m_ecs, m_eventSystem, id, m_playerId, 0);
      sysBehaviour.addBehaviour(id, std::move(collectBehaviour));

      auto dissolveBehaviour = fge::createBGeneric(hashString("dissolve"), { g_strEntityExplode },
        [&sysAnimation, this, id](const Event& e) {

        if (e.name == g_strEntityExplode) {
          if (sysAnimation.hasAnimationPlaying(id)) {
            sysAnimation.finishAnimation(id);
          }
          sysAnimation.playAnimation(id, hashString("collect"), [this, id]() {
            m_eventSystem.raiseEvent(fge::ERequestDeletion{id});
          });
        }
      });

      sysBehaviour.addBehaviour(id, std::move(dissolveBehaviour));
    }
  }
}

std::set<std::pair<int, int>> SceneBuilderImpl::constructMines(uint32_t numMines)
{
  static auto strExplode = hashString("explode");

  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();

  auto makeFrame = [](float tx, float ty) {
    return Animation2dFrame{
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

  auto animExplode = std::unique_ptr<Animation2d>(new Animation2d{
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
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();

    m_entities.insert(id);

    int x = coords[i][0];
    int y = coords[i][1];

    sysGrid.addEntity(id, x, y);

    Vec2f size{ GRID_CELL_W, GRID_CELL_H };
    Vec2f pos{ GRID_CELL_W * x, GRID_CELL_H * y };

    DSpatial spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
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
          sysGrid.removeEntity(id);

          EntityIdSet targets = sysGrid.getEntities(x - 1, y - 1, x + 1, y + 1);
          m_eventSystem.raiseEvent(EEntityExplode{id, event.pos, targets});

          sysAnimation.playAnimation(id, strExplode, [this, id]() {
            m_eventSystem.raiseEvent(fge::ERequestDeletion{id});
          });
          sysRender.setZIndex(id, static_cast<uint32_t>(ZIndex::Explosion));
        }
      }
    };

    auto behaviour = fge::createBGeneric(strExplode, { g_strEntityLandOn }, onEvent);
    sysBehaviour.addBehaviour(id, std::move(behaviour));

    sysAnimation.addEntity(id, DAnimation2d{
      .animations = { animExplodeId }
    });

    mines.insert({ x, y });
  }

  return mines;
}

void SceneBuilderImpl::constructNumericTiles(const std::set<std::pair<int, int>>& mines)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();

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
        CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
      >();

      m_entities.insert(id);

      sysGrid.addEntity(id, i, j);

      Vec2f size{ GRID_CELL_W, GRID_CELL_H };
      Vec2f pos{ GRID_CELL_W * i, GRID_CELL_H * j };

      DSpatial spatial{
        .transform = spriteTransform(pos, size),
        .parent = m_worldRoot,
        .enabled = true
      };

      sysSpatial.addEntity(id, spatial);

      DSprite render{
        .scissor = MAIN_SCISSOR,
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

      auto behaviour = createBNumericTile(m_ecs, m_eventSystem, id, Vec2i{ i, j }, value);

      sysBehaviour.addBehaviour(id, std::move(behaviour));
    }
  }
}

void SceneBuilderImpl::constructGradient()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
  >();

  m_entities.insert(id);

  Vec2f size{ GRID_CELL_W * GRID_W, GRID_CELL_H * static_cast<float>(GRID_H + 0.5f) };
  Vec2f pos{ 0.f, 0.f };

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DSprite render{
    .scissor = MAIN_SCISSOR,
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

void SceneBuilderImpl::constructBackQuad()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CQuad
  >();

  m_entities.insert(id);

  Vec2f size{ GRID_CELL_W * GRID_W, GRID_CELL_H * (GRID_H + 4)};
  Vec2f pos{ 0.f, 0.f };

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DQuad render{
    .scissor = MAIN_SCISSOR,
    .zIndex = static_cast<uint32_t>(ZIndex::Background),
    .colour = { 0.f, 0.f, 0.f, 1.f },
    .radius = 0.f
  };

  sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructCoins(uint32_t numCoins)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  auto makeFrame = [](float tx, float ty, float a) {
    return Animation2dFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      // Tweak the size and position a bit
      .textureRect = Rectf{
        .x = pxToUvX(tx),
        .y = pxToUvY(ty - 1, 16 + 1.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16 + 1.f)
      },
      .colour = Vec4f{ 1.f, 1.f, 1.f, a }
    };
  };

  auto animIdle = std::unique_ptr<Animation2d>(new Animation2d{
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

  auto animCollect = std::unique_ptr<Animation2d>(new Animation2d{
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
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();

    m_entities.insert(id);

    int x = coords[i][0];
    int y = coords[i][1];

    sysGrid.addEntity(id, x, y);

    Vec2f size{ 0.03f, 0.03f };
    Vec2f offset{ (GRID_CELL_W - size[0]) * 0.5f, (GRID_CELL_H - size[1]) * 0.5f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    DSpatial spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
      .textureRect = Rectf{
        .x = pxToUvX(960.f),
        .y = pxToUvY(0.f, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Coin)
    };

    sysRender.addEntity(id, render);

    sysAnimation.addEntity(id, DAnimation2d{
      .animations = { animIdleId, animCollectId }
    });

    sysAnimation.playAnimation(id, hashString("idle"), true);
    sysAnimation.seek(id, fge::randomInt());

    auto behaviour = createBCollectable(m_ecs, m_eventSystem, id, m_playerId, 1);
    sysBehaviour.addBehaviour(id, std::move(behaviour));
  }
}

void SceneBuilderImpl::constructGoldNuggets(uint32_t numNuggets,
  const std::set<std::pair<int, int>>& mines)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  auto makeFrame = [](float tx, float ty, float a) {
    return Animation2dFrame{
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

  auto animIdle = std::unique_ptr<Animation2d>(new Animation2d{
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

  auto animCollect = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("collect"),
    .duration = 12,
    .frames = {
      makeFrame(960, 64, 1.f),
      makeFrame(960, 64, 0.f)
    }
  });

  auto animCollectId = sysAnimation.addAnimation(std::move(animCollect));

  auto coords = randomGridCoords();

  size_t numConstructed = 0;
  for (size_t i = 0; i < coords.size(); ++i) {
    assert(i < coords.size());

    if (numConstructed >= numNuggets) {
      break;
    }

    int x = coords[i][0];
    int y = coords[i][1];

    if (mines.contains({ x, y })) {
      continue;
    }

    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();

    m_entities.insert(id);

    sysGrid.addEntity(id, x, y);

    Vec2f size{ 0.03f, 0.03f };
    Vec2f offset{ (GRID_CELL_W - size[0]) * 0.5f, (GRID_CELL_H - size[1]) * 0.5f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    DSpatial spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
      .textureRect = Rectf{
        .x = pxToUvX(960.f),
        .y = pxToUvY(64.f, 32.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Nugget)
    };

    sysRender.addEntity(id, render);

    sysAnimation.addEntity(id, DAnimation2d{
      .animations = { animIdleId, animCollectId }
    });

    sysAnimation.playAnimation(id, hashString("idle"), true);
    sysAnimation.seek(id, fge::randomInt());

    auto behaviour = createBCollectable(m_ecs, m_eventSystem, id, m_playerId, 5);
    sysBehaviour.addBehaviour(id, std::move(behaviour));

    ++numConstructed;
  }
}

void SceneBuilderImpl::constructWanderers(uint32_t numWanderers,
  const std::set<std::pair<int, int>>& mines)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  auto makeFrame = [](float x, float y, float tx, float ty, float a) {
    return Animation2dFrame{
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

  auto animFadeIn = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("fade_in"),
    .duration = 30,
    .frames = {
      makeFrame(0.f, 0.f, 256.f, 400.f, 0.f),
      makeFrame(0.f, 0.f, 256.f, 400.f, 1.f)
    }
  });

  auto animFadeInId = sysAnimation.addAnimation(std::move(animFadeIn));

  long animationDuration = 32;
  float delta = 0.015625f;

  auto animMoveLeft = std::unique_ptr<Animation2d>(new Animation2d{
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

  auto animMoveRight = std::unique_ptr<Animation2d>(new Animation2d{
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

  auto animMoveUp = std::unique_ptr<Animation2d>(new Animation2d{
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

  auto animMoveDown = std::unique_ptr<Animation2d>(new Animation2d{
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

  size_t numConstructed = 0;
  for (size_t i = 0; i < coords.size(); ++i) {
    assert(i < coords.size());

    if (numConstructed >= numWanderers) {
      break;
    }

    int x = coords[i][0];
    int y = coords[i][1];

    if (x < 6 && y < 6) {
      continue;
    }

    if (mines.contains({ x, y })) {
      continue;
    }

    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();

    m_entities.insert(id);

    sysGrid.addEntity(id, x, y);

    Vec2f size{ 0.0625, 0.0625f };
    Vec2f offset{ (GRID_CELL_W - size[0]) * 0.5f, (GRID_CELL_H - size[1]) * 0.5f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    DSpatial spatial{
      .transform = spriteTransform(pos, size),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
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

    sysAnimation.addEntity(id, DAnimation2d{
      .animations = {
        animFadeInId,
        animMoveLeftId,
        animMoveRightId,
        animMoveUpId,
        animMoveDownId
      }
    });

    auto behaviour = createBWanderer(m_ecs, m_eventSystem, id, m_playerId);
    sysBehaviour.addBehaviour(id, std::move(behaviour));

    ++numConstructed;
  }
}

void SceneBuilderImpl::constructSticks(uint32_t numSticks,
  const std::set<std::pair<int, int>>& mines)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  auto animThrow = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("throw"),
    .duration = 30,
    .frames = {
      Animation2dFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
      }
    }
  });

  auto animThrowId = sysAnimation.addAnimation(std::move(animThrow));

  auto animFade = std::unique_ptr<Animation2d>(new Animation2d{
    .name = hashString("fade"),
    .duration = 30,
    .frames = {
      Animation2dFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.f }
      }
    }
  });

  auto animFadeId = sysAnimation.addAnimation(std::move(animFade));

  auto coords = randomGridCoords();

  size_t numConstructed = 0;
  for (size_t i = 0; i < coords.size(); ++i) {
    assert(i < coords.size());

    if (numConstructed >= numSticks) {
      break;
    }

    int x = coords[i][0];
    int y = coords[i][1];

    if (mines.contains({ x, y })) {
      continue;
    }

    auto id = m_ecs.componentStore().allocate<
      CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
    >();

    m_entities.insert(id);

    sysGrid.addEntity(id, x, y);

    Vec2f size{ GRID_CELL_W, GRID_CELL_H };
    Vec2f offset{ 0.5f * GRID_CELL_W, 0.f };
    Vec2f pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y } + offset;

    DSpatial spatial{
      .transform = spriteTransform(pos, size, fge::PIf / 4.f, { 0.f, 0.5f }),
      .parent = m_worldRoot,
      .enabled = true
    };

    sysSpatial.addEntity(id, spatial);

    DSprite render{
      .scissor = MAIN_SCISSOR,
      .textureRect = Rectf{
        .x = pxToUvX(960.f),
        .y = pxToUvY(160.f, 32.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(32.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::Stick)
    };

    sysRender.addEntity(id, render);

    sysAnimation.addEntity(id, DAnimation2d{
      .animations = { animThrowId, animFadeId }
    });

    auto behaviour = createBStick(m_ecs, m_eventSystem, id, m_playerId, animThrowId);
    sysBehaviour.addBehaviour(id, std::move(behaviour));

    ++numConstructed;
  }
}

void SceneBuilderImpl::constructExit()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  auto makeFrame = [](float tx, float ty) {
    return Animation2dFrame{
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

  auto animIdle = std::unique_ptr<Animation2d>(new Animation2d{
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
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
  >();

  m_entities.insert(id);

  Vec2f size{ 0.0625f, 0.0625f };
  Vec2f pos{ GRID_CELL_W * (GRID_W - 1), GRID_CELL_H * (GRID_H - 1) };

  sysGrid.addEntity(id, (GRID_W - 1), (GRID_H - 1));

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DSprite render{
    .scissor = MAIN_SCISSOR,
    .textureRect = Rectf{
      .x = pxToUvX(128.f),
      .y = pxToUvY(416.f, 16.f),
      .w = pxToUvW(16.f),
      .h = pxToUvH(16.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Exit)
  };

  sysRender.addEntity(id, render);

  sysAnimation.addEntity(id, DAnimation2d{ .animations = { animIdleId } });

  auto behaviour = createBExit(m_ecs, m_eventSystem, id,  m_playerId);

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

void SceneBuilderImpl::constructTimeCounter(uint32_t timeAvailable)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite, CDynamicText
  >();

  m_entities.insert(id);

  Vec2f size{ 0.035f, 0.035f };
  Vec2f pos{ 0.07f, 0.94f };

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DDynamicText render{
    .scissor = MAIN_SCISSOR,
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .text = std::to_string(timeAvailable),
    .maxLength = 3,
    .zIndex = static_cast<uint32_t>(ZIndex::Hud),
    .colour = Vec4f{ 0.3f, 0.7f, 0.3f, 1.f }
  };

  sysRender.addEntity(id, render);

  auto behaviour = fge::createBGeneric(hashString("on_tick"), { g_strTimerTick },
    [this, &sysRender, id](const Event& e) {

    if (e.name == g_strTimerTick) {
      auto& event = dynamic_cast<const ETimerTick&>(e);

      sysRender.updateDynamicText(id, std::to_string(event.timeRemaining));

      if (event.timeRemaining == 10) {
        m_ecs.componentStore().component<CRender2d>(id).colour = { 1.f, 0.f, 0.f, 1.f };
        m_eventSystem.raiseEvent(ETenSecondsRemaining{});
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

void SceneBuilderImpl::constructCoinCounter(uint32_t goldRequired)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite, CDynamicText
  >();

  m_entities.insert(id);

  Vec2f size{ 0.035f, 0.035f };
  Vec2f pos{ 0.27f, 0.94f };

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DDynamicText render{
    .scissor = MAIN_SCISSOR,
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .text = std::to_string(goldRequired),
    .maxLength = 3,
    .zIndex = static_cast<uint32_t>(ZIndex::Hud),
    .colour = Vec4f{ 0.3f, 0.7f, 0.3f, 1.f }
  };

  sysRender.addEntity(id, render);

  auto behaviour = createBCoinCounter(goldRequired, m_ecs, m_eventSystem, id);
  sysBehaviour.addBehaviour(id, std::move(behaviour));
}

EntityId SceneBuilderImpl::constructStaticEntity(const Vec2f& pos, const Vec2f& size,
  const Rectf& texRect, uint32_t zIndex)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
  >();

  m_entities.insert(id);

  DSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_worldRoot,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DSprite render{
    .scissor = MAIN_SCISSOR,
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

  m_ecs.componentStore().component<CRender2d>(id).visible = false;

  return id;
}

EntityId SceneBuilderImpl::constructRestartGamePrompt()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender2d, CSprite
  >();

  m_entities.insert(id);

  Vec2f pos{ 0.15f, 0.01f };
  Vec2f charSize{ 0.024f, 0.04f };

  DSpatial spatial{
    .transform = spriteTransform(pos, charSize),
    .parent = m_worldRoot,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  DText render{
    .scissor = MAIN_SCISSOR,
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .text = "Tap screen or press any button to restart",
    .zIndex = static_cast<uint32_t>(ZIndex::Hud),
    .colour = { 0.3f, 0.7f, 0.3f, 1.f }
  };

  sysRender.addEntity(id, render);

  return id;
}

} // namespace

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, Ecs& ecs,
  const GameOptionsManager& options)
{
  return std::make_unique<SceneBuilderImpl>(eventSystem, ecs, options);  
}
