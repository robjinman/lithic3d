#include "audio_system.hpp"
#include <iostream> // TODO

namespace
{

class AudioSystemImpl : public AudioSystem
{
  public:
    void addSound(HashedString name, const std::string& path) override;
    void playSound(HashedString name) override;
};

void AudioSystemImpl::addSound(HashedString name, const std::string& path)
{

}

void AudioSystemImpl::playSound(HashedString name)
{
  std::cout << "Playing sound: " << getHashedString(name) << "\n";
}

} // namespace

AudioSystemPtr createAudioSystem()
{
  return std::make_unique<AudioSystemImpl>();
}
