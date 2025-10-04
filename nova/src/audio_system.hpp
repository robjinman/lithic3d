#pragma once

#include "utils.hpp"
#include <memory>

class AudioSystem
{
  public:
    virtual void addSound(HashedString name, const std::string& path) = 0;
    virtual void playSound(HashedString name) = 0;

    virtual ~AudioSystem() = default;
};

using AudioSystemPtr = std::unique_ptr<AudioSystem>;

class FileSystem;

AudioSystemPtr createAudioSystem(FileSystem& fileSystem);
