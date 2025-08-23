#include "scene_builder.hpp"
#include "sys_animation.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_ui.hpp"
#include "b_collectable.hpp"
#include "b_generic.hpp"
#include "game_events.hpp"
#include "player.hpp"
#include <random>

namespace
{

class SceneBuilderImpl : public SceneBuilder
{
  public:
    SceneBuilderImpl(EventSystem& eventSystem, ComponentStore& componentStore,
      SysAnimation& sysAnimation, SysBehaviour& sysBehaviour, SysGrid& sysGrid,
      SysRender& sysRender);

    EntityId buildScene() override;

  private:
    EventSystem& m_eventSystem;
    ComponentStore&  m_componentStore;
    SysAnimation& m_sysAnimation;
    SysBehaviour& m_sysBehaviour;
    SysGrid& m_sysGrid;
    SysRender& m_sysRender;
    EntityId m_playerId;

    void constructMenus();
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

SceneBuilderImpl::SceneBuilderImpl(EventSystem& eventSystem, ComponentStore& componentStore,
  SysAnimation& sysAnimation, SysBehaviour& sysBehaviour, SysGrid& sysGrid,
  SysRender& sysRender)
  : m_eventSystem(eventSystem)
  , m_componentStore(componentStore)
  , m_sysAnimation(sysAnimation)
  , m_sysBehaviour(sysBehaviour)
  , m_sysGrid(sysGrid)
  , m_sysRender(sysRender)
{
}

EntityId SceneBuilderImpl::buildScene()
{
  m_playerId = constructPlayer(m_eventSystem, m_componentStore, m_sysGrid, m_sysRender,
    m_sysBehaviour, m_sysAnimation);
  constructMenus();
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

  return m_playerId;
}

void SceneBuilderImpl::constructMenus()
{

}

void SceneBuilderImpl::constructSky()
{
  auto id = m_componentStore.allocate<CRenderView>();

  CRender render{
    .textureRect = Rectf{
      .x = pxToUvX(0.f),
      .y = pxToUvY(416.f, 32.f),
      .w = pxToUvW(128.f),
      .h = pxToUvH(32.f)
    },
    .size = Vec2f{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H },
    .pos = Vec2f{ 0.f, 12.f * GRID_CELL_H },
    .zIndex = 0
  };

  m_sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructClouds()
{
  static auto strIdle = hashString("idle");
  long animationDuration = 7200;

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = hashString("idle"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .delta = Vec2f{ -2.f * GRID_W * GRID_CELL_W, 0.f },
        .textureRect = std::nullopt,
        .colour = std::nullopt
      }
    }
  });

  auto animIdleId = m_sysAnimation.addAnimation(std::move(animIdle));

  {
    auto id = m_componentStore.allocate<CRenderView>();

    CRender render{
      .textureRect = Rectf{
        .x = pxToUvX(256.f),
        .y = pxToUvY(0.f, 32.f),
        .w = pxToUvW(128.f),
        .h = pxToUvH(32.f)
      },
      .size = Vec2f{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H },
      .pos = Vec2f{ GRID_CELL_W * GRID_W, 0.8f },
      .zIndex = 1,
      .colour = Vec4f{ 1.f, 0.8f, 0.5f, 0.6f }
    };

    m_sysRender.addEntity(id, render);

    CAnimation animation{
      .animations = { animIdleId }
    };

    m_sysAnimation.addEntity(id, animation);

    m_sysAnimation.playAnimation(id, strIdle, true);
    m_sysAnimation.seek(id, animationDuration / 2);
  }

  {
    auto id = m_componentStore.allocate<CRenderView>();

    CRender render{
      .textureRect = Rectf{
        .x = pxToUvX(256.f),
        .y = pxToUvY(32.f, 32.f),
        .w = pxToUvW(128.f),
        .h = pxToUvH(32.f)
      },
      .size = Vec2f{ GRID_CELL_W * GRID_W, 4.f * GRID_CELL_H },
      .pos = Vec2f{ GRID_CELL_W * GRID_W, 0.8f },
      .zIndex = 1,
      .colour = Vec4f{ 1.f, 0.8f, 0.5f, 0.6f }
    };

    m_sysRender.addEntity(id, render);

    CAnimation animation{
      .animations = { animIdleId }
    };

    m_sysAnimation.addEntity(id, animation);

    m_sysAnimation.playAnimation(id, strIdle, true);
  }
}

void SceneBuilderImpl::constructTrees()
{
  auto id = m_componentStore.allocate<CRenderView>();

  CRender render{
    .textureRect = Rectf{
      .x = pxToUvX(0.f),
      .y = pxToUvY(352.f, 40.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(40.f)
    },
    .size = Vec2f{ GRID_CELL_W * GRID_W, 3.f * GRID_CELL_H },
    .pos = Vec2f{ 0.f, 11.f * GRID_CELL_H },
    .zIndex = 2
  };

  m_sysRender.addEntity(id, render);
}

void SceneBuilderImpl::constructFakeSoil()
{
  for (size_t i = 0; i < GRID_W; ++i) {
    auto id = m_componentStore.allocate<CRenderView>();

    float_t x = GRID_CELL_W * i;

    CRender render{
      .textureRect = Rectf{
        .x = pxToUvX(384.f),
        .y = pxToUvY(0.f, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      },
      .size = Vec2f{ GRID_CELL_W, GRID_CELL_H },
      .pos = Vec2f{ x, 11.f * GRID_CELL_H },
      .zIndex = 1
    };

    m_sysRender.addEntity(id, render);
  }
}

void SceneBuilderImpl::constructSoil()
{
  long animationDuration = 16;

  auto animCollect = std::unique_ptr<Animation>(new Animation{
    .name = hashString("collect"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.875f }
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.75f }
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.625f }
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.5f }
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.375f }
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .colour = Vec4f{ 1.f, 1.f, 1.f, 0.25f }
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
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

  auto animCollectId = m_sysAnimation.addAnimation(std::move(animCollect));

  for (size_t j = 0; j < GRID_H; ++j) {
    for (size_t i = 0; i < GRID_W; ++i) {
      if ((i < 2 && j < 2) || (i > GRID_W - 3 && j > GRID_H - 3)) {
        continue;
      }

      auto id = m_componentStore.allocate<CRenderView>();

      m_sysGrid.addEntity(id, i, j);

      float_t x = GRID_CELL_W * i;
      float_t y = GRID_CELL_H * j;

      CRender render{
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(0.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .size = Vec2f{ GRID_CELL_W, GRID_CELL_H },
        .pos = Vec2f{ x, y },
        .zIndex = 1
      };

      m_sysRender.addEntity(id, render);

      m_sysAnimation.addEntity(id, CAnimation{
        .animations = { animCollectId }
      });

      auto behaviour = createBCollectable(m_sysAnimation, m_eventSystem, id, m_playerId, 0);
      m_sysBehaviour.addBehaviour(id, std::move(behaviour));
    }
  }
}

std::set<std::pair<int, int>> SceneBuilderImpl::constructMines()
{
  static auto strExplode = hashString("explode");
  size_t numMines = 40; // TODO

  auto animExplode = std::unique_ptr<Animation>(new Animation{
    .name = hashString("explode"),
    .duration = 30,
    .frames = {
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(576.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(640.f),
          .y = pxToUvY(0.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(576.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(640.f),
          .y = pxToUvY(64.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(576.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(640.f),
          .y = pxToUvY(128.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(192.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(512.f),
          .y = pxToUvY(192.f, 64.f),
          .w = pxToUvW(64.f),
          .h = pxToUvH(64.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.f },
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

  auto animExplodeId = m_sysAnimation.addAnimation(std::move(animExplode));

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
    auto id = m_componentStore.allocate<CRenderView>();

    int x = coords[i][0];
    int y = coords[i][1];

    m_sysGrid.addEntity(id, x, y);

    CRender render{
      .textureRect = Rectf{
        .x = pxToUvX(672.f),
        .y = pxToUvY(224.f, 32.f),
        .w = pxToUvW(32.f),
        .h = pxToUvH(32.f)
      },
      .size = Vec2f{ GRID_CELL_W, GRID_CELL_H },
      .pos = Vec2f{ GRID_CELL_W * x, GRID_CELL_H * y },
      .zIndex = 0
    };

    m_sysRender.addEntity(id, render);

    auto onStepOn = [this, id, x, y](const GameEvent& e) {
      if (e.name == g_strEntityStepOn) {
        auto& event = dynamic_cast<const EEntityStepOn&>(e);

        Vec2i pos{ x, y };
        if (event.toPos == pos) {
          EntityIdSet targets = m_sysGrid.getEntities(x - 1, y - 1, x + 1, y + 1);

          m_eventSystem.queueEvent(std::make_unique<EEntityExplode>(id, pos, targets));

          m_sysAnimation.playAnimation(id, strExplode);
        }
      }
      else if (e.name == g_strAnimationFinished) {
        auto& event = dynamic_cast<const EAnimationFinished&>(e);

        if (event.entityId == id && event.animationName == strExplode) {
          m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(id));
        }
      }
    };

    auto explode = createGenericBehaviour(strExplode, { g_strEntityStepOn }, onStepOn);
    m_sysBehaviour.addBehaviour(id, std::move(explode));

    m_sysAnimation.addEntity(id, CAnimation{
      .animations = { animExplodeId }
    });

    mines.insert({ x, y });
  }

  return mines;
}

void SceneBuilderImpl::constructNumbers(const std::set<std::pair<int, int>>& mines)
{
  std::array<std::array<int, GRID_W>, GRID_H> numbers{};

  auto inRange = [](int x, int y) {
    return x >= 0 && x < GRID_W && y >= 0 && y < GRID_H;
  };

  for (auto& mine : mines) {
    for (int i = -1; i <= 1; ++i) {
      for (int j = -1; j <= 1; ++j) {
        if (i == 0 && j == 0) {
          continue;
        }

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
      if (mines.contains({ i, j })) {
        continue;
      }

      auto id = m_componentStore.allocate<CRenderView>();

      CRender render{
        .textureRect = Rectf{
          .x = pxToUvX(16.f * (value - 1)),
          .y = pxToUvY(400.f, 16.f),
          .w = pxToUvW(16.f),
          .h = pxToUvH(16.f)
        },
        .size = Vec2f{ GRID_CELL_W, GRID_CELL_H },
        .pos = Vec2f{ GRID_CELL_W * i, GRID_CELL_H * j },
        .zIndex = 0
      };

      m_sysRender.addEntity(id, render);
    }
  }
}

void SceneBuilderImpl::constructGradient()
{
  auto id = m_componentStore.allocate<CRenderView>();

  CRender render{
    .textureRect = Rectf{
      .x = pxToUvX(512.f),
      .y = pxToUvY(256.f, 128.f),
      .w = pxToUvW(128.f),
      .h = pxToUvH(128.f)
    },
    .size = Vec2f{ GRID_CELL_W * GRID_W, GRID_CELL_H * (5 + GRID_H) },
    .pos = Vec2f{ 0.f, 0.f },
    .zIndex = 9
  };

  m_sysRender.addEntity(id, render);
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

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, ComponentStore& componentStore,
  SysAnimation& sysAnimation, SysBehaviour& sysBehaviour, SysGrid& sysGrid,
  SysRender& sysRender)
{
  return std::make_unique<SceneBuilderImpl>(eventSystem, componentStore, sysAnimation, sysBehaviour,
    sysGrid, sysRender);  
}
