#pragma once

#include "units.hpp"
#include <memory>

namespace lithic3d
{

struct GameConfig;
class Game;
class Logger;
class FileSystem;
class EventSystem;
class AudioSystem;
namespace render { class Renderer; }
class Ecs;
struct InputState;
class ModelLoader;
class ResourceManager;

class Engine
{
  public:
    virtual void setClearColour(const Vec4f& colour) = 0;
    virtual void update(const InputState& inputState) = 0;
    virtual void onWindowResize() = 0;
    virtual Tick currentTick() const = 0;
    virtual float measuredTickRate() const = 0;

    virtual Logger& logger() = 0;
    virtual FileSystem& fileSystem() = 0;
    virtual EventSystem& eventSystem() = 0;
    virtual render::Renderer& renderer() = 0;
    virtual AudioSystem& audioSystem() = 0;
    virtual Ecs& ecs() = 0;
    virtual ModelLoader& modelLoader() = 0;
    virtual ResourceManager& resourceManager() = 0;

    virtual ~Engine() = default;
};

using EnginePtr = std::unique_ptr<Engine>;

EnginePtr createEngine(std::unique_ptr<render::Renderer> renderer,
  std::unique_ptr<AudioSystem> audioSystem, std::unique_ptr<FileSystem> fileSystem,
  std::unique_ptr<Logger> logger);

} // namespace lithic3d
