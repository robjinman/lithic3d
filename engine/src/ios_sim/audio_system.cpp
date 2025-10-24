#include <fge/audio_system.hpp>

namespace fge
{
namespace
{

class AudioSystemImpl : public AudioSystem
{
  public:
    AudioSystemImpl(FileSystem&) {}

    void addSound(HashedString name, const std::string& path) override {}
    void playSound(HashedString name) override {}
    void stopAllSounds() override {}
    void setSoundVolume(float volume) override {}

    void addMusic(const std::string& path) override {}
    void playMusic() override {}
    void stopMusic() override {}
    void setMusicVolume(float volume) override {}

    ~AudioSystemImpl() {}
};

} // namespace

AudioSystemPtr createAudioSystem(FileSystem& fileSystem)
{
  return std::make_unique<AudioSystemImpl>(fileSystem);
}

} // namespace fge
