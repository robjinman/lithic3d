#pragma once

#include <memory>

namespace fge
{

class GameConfig;
class Game;
class Logger;
class FileSystem;
class EventSystem;
class AudioSystem;
namespace render { class Renderer; }
class Ecs;

class Engine
{
  public:
    virtual Logger& logger() = 0;
    virtual FileSystem& fileSystem() = 0;
    virtual EventSystem& eventSystem() = 0;
    virtual render::Renderer& renderer() = 0;
    virtual AudioSystem& audioSystem() = 0;
    virtual Ecs& ecs() = 0;

    virtual ~Engine() = default;
};

using EnginePtr = std::unique_ptr<Engine>;

EnginePtr createEngine(std::unique_ptr<render::Renderer> renderer,
  std::unique_ptr<AudioSystem> audioSystem, std::unique_ptr<FileSystem> fileSystem,
  std::unique_ptr<Logger> logger);

} // namespace fge
