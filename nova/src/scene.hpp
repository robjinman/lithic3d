#pragma once

#include "player.hpp"
#include <memory>

class SpatialSystem;
class RenderSystem;
class FileSystem;
class Logger;

PlayerPtr createScene(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
  const FileSystem& fileSystem, Logger& logger);
