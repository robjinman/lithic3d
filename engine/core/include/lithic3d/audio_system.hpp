#pragma once

#include "strings.hpp"
#include "file_system.hpp"
#include <memory>

namespace lithic3d
{

/*
enum class AudioResourceType
{
  Sound,
  Music
};

class AudioResource : public ResourceData
{
  public:
    AudioResourceType type;
    HashedString name;
    std::filesystem::path filePath;
};
*/

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

class Logger;

AudioSystemPtr createAudioSystem(DirectoryPtr directory, Logger& logger);

} // namespace lithic3d
