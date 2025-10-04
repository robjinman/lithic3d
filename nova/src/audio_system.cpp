#include "audio_system.hpp"
#include "exception.hpp"
#include "file_system.hpp"
#include <AL/al.h>
#include <AL/alc.h>
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>
#include <map>

namespace
{

constexpr int NUM_SOURCES = 4;

class AudioSystemImpl : public AudioSystem
{
  public:
    AudioSystemImpl(FileSystem& fileSystem);

    void addSound(HashedString name, const std::string& path) override;
    void playSound(HashedString name) override;

    ~AudioSystemImpl();

  private:
    FileSystem& m_fileSystem;
    ALCdevice* m_device;
    ALCcontext* m_context;
    std::array<ALuint, NUM_SOURCES> m_sources;
    std::map<HashedString, ALuint> m_buffers;

    ALuint getFreeSource() const;
};

AudioSystemImpl::AudioSystemImpl(FileSystem& fileSystem)
  : m_fileSystem(fileSystem)
{
  m_device = alcOpenDevice(nullptr);
  if (!m_device) {
    EXCEPTION("Failed to open OpenAL device");
  }

  m_context = alcCreateContext(m_device, nullptr);
  if (!m_context || !alcMakeContextCurrent(m_context)) {
    EXCEPTION("Failed to create/make current OpenAL context");
  }

  alGenSources(m_sources.size(), m_sources.data());

  for (auto source : m_sources) {
    alSourcef(source, AL_GAIN, 1.0f);
    alSource3f(source, AL_POSITION, 0.f, 0.f, 0.f);
  }
}

AudioSystemImpl::~AudioSystemImpl()
{
  for (auto source : m_sources) {
    alSourceStop(source);
  }
  alDeleteSources(m_sources.size(), m_sources.data());

  for (auto entry : m_buffers) {
    alDeleteBuffers(1, &entry.second);
  }

  alcMakeContextCurrent(nullptr);
  alcDestroyContext(m_context);
  alcCloseDevice(m_device);
}

ALuint AudioSystemImpl::getFreeSource() const
{
  for (auto source : m_sources) {
    ALint state = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &state);

    if (state != AL_PLAYING) {
      return source;
    }
  }

  EXCEPTION("No free sources");
}

void AudioSystemImpl::addSound(HashedString name, const std::string& path)
{
  auto data = m_fileSystem.readFile(path);

  drwav wav;
  if (!drwav_init_memory(&wav, data.data(), data.size(), nullptr)) {
    EXCEPTION("Failed to open wav file");
  }

  ALenum format = 0;
  if (wav.channels == 1) {
    format = AL_FORMAT_MONO16;
  }
  else if (wav.channels == 2) {
    format = AL_FORMAT_STEREO16;
  }
  else {
    EXCEPTION("Unsupported number of channels");
  }

  std::vector<int16_t> pcmData;
  pcmData.resize(static_cast<size_t>(wav.totalPCMFrameCount) * wav.channels);

  drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pcmData.data());

  ALuint buffer = 0;
  alGenBuffers(1, &buffer);
  alBufferData(buffer, format, pcmData.data(), pcmData.size() * sizeof(int16_t), wav.sampleRate);

  m_buffers.insert({ name, buffer });

  drwav_uninit(&wav);
}

void AudioSystemImpl::playSound(HashedString name)
{
  auto buffer = m_buffers.at(name);
  auto source = getFreeSource();

  alSourcei(source, AL_BUFFER, buffer);
  alSourcePlay(source);
}

} // namespace

AudioSystemPtr createAudioSystem(FileSystem& fileSystem)
{
  return std::make_unique<AudioSystemImpl>(fileSystem);
}
