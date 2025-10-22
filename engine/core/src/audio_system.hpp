#pragma once

#include "strings.hpp"
#include <memory>

class AudioSystem
{
  public:
    virtual void addSound(HashedString name, const std::string& path) = 0;
    virtual void playSound(HashedString name) = 0;
    virtual void stopAllSounds() = 0;
    virtual void setSoundVolume(float volume) = 0;

    virtual void addMusic(const std::string& path) = 0;
    virtual void playMusic() = 0;
    virtual void stopMusic() = 0;
    virtual void setMusicVolume(float volume) = 0;

    virtual ~AudioSystem() = default;
};

using AudioSystemPtr = std::unique_ptr<AudioSystem>;

class FileSystem;

AudioSystemPtr createAudioSystem(FileSystem& fileSystem);
