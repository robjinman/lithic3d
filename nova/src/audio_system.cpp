#include "audio_system.hpp"
#include "exception.hpp"
#include "file_system.hpp"
#include "stb_vorbis.hpp"
#include <AL/al.h>
#include <AL/alc.h>
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>
#include <map>

namespace
{

constexpr int NUM_SOURCES = 4;

ALenum chooseFormat(ALuint channels)
{
  if (channels == 1) {
    return AL_FORMAT_MONO16;
  }
  else if (channels == 2) {
    return AL_FORMAT_STEREO16;
  }
  else {
    EXCEPTION("Unsupported number of channels");
  }
}

class AudioSystemImpl : public AudioSystem
{
  public:
    AudioSystemImpl(FileSystem& fileSystem);

    void addSound(HashedString name, const std::string& path) override;
    void playSound(HashedString name) override;
    void stopAllSounds() override;
    void setSoundVolume(float volume) override;

    void addMusic(const std::string& path) override;
    void playMusic() override;
    void stopMusic() override;
    void setMusicVolume(float volume) override;

    ~AudioSystemImpl();

  private:
    FileSystem& m_fileSystem;
    ALCdevice* m_device;
    ALCcontext* m_context;
    std::array<ALuint, NUM_SOURCES> m_sources;
    std::map<HashedString, ALuint> m_buffers;
    ALuint m_musicBuffer = 0;
    ALuint m_musicSource = 0;
    float m_soundVolume = 0.75f;
    float m_musicVolume = 0.75f;

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
    alSourcef(source, AL_GAIN, m_soundVolume);
    alSource3f(source, AL_POSITION, 0.f, 0.f, 0.f);
  }

  alGenSources(1, &m_musicSource);
  alSource3f(m_musicSource, AL_POSITION, 0.f, 0.f, 0.f);
  alSourcef(m_musicSource, AL_GAIN, m_musicVolume);
  alSourcei(m_musicSource, AL_LOOPING, AL_TRUE);
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

  ALenum format = chooseFormat(wav.channels);

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

void AudioSystemImpl::stopAllSounds()
{
  for (auto source : m_sources) {
    alSourceStop(source);
  }
}

void AudioSystemImpl::setSoundVolume(float volume)
{
  m_soundVolume = volume;

  for (auto source : m_sources) {
    alSourcef(source, AL_GAIN, m_soundVolume);
  }
}

void AudioSystemImpl::setMusicVolume(float volume)
{
  m_musicVolume = volume;

  alSourcef(m_musicSource, AL_GAIN, m_musicVolume);
}

void AudioSystemImpl::addMusic(const std::string& path)
{
  if (!m_musicBuffer != 0) {
    stopMusic();
    alDeleteBuffers(1, &m_musicBuffer);
    m_musicBuffer = 0;
  }

  auto data = m_fileSystem.readFile(path);

  int channels = 0;
  int rate = 0;
  int16_t* decoded = nullptr;

  int frames = stb_vorbis_decode_memory(reinterpret_cast<const unsigned char*>(data.data()),
    static_cast<int>(data.size()), &channels, &rate, &decoded);

  if (frames < 0) {
    EXCEPTION("Failed to load ogg file");
  }

  std::vector<int16_t> pcmData;
  pcmData.assign(decoded, decoded + frames * channels);

  free(decoded);

  ALsizei sampleRate = static_cast<ALsizei>(rate);
  ALenum format = chooseFormat(channels);

  alGenBuffers(1, &m_musicBuffer);
  alBufferData(m_musicBuffer, format, pcmData.data(),
    static_cast<ALsizei>(pcmData.size() * sizeof(int16_t)), sampleRate);

  alSourcei(m_musicSource, AL_BUFFER, m_musicBuffer);
}

void AudioSystemImpl::playMusic()
{
  alSourcePlay(m_musicSource);
}

void AudioSystemImpl::stopMusic()
{
  alSourceStop(m_musicSource);
}

AudioSystemImpl::~AudioSystemImpl()
{
  stopAllSounds();
  alDeleteSources(m_sources.size(), m_sources.data());

  for (auto entry : m_buffers) {
    alDeleteBuffers(1, &entry.second);
  }

  stopMusic();

  alDeleteSources(1, &m_musicSource);
  alDeleteBuffers(1, &m_musicBuffer);

  m_musicBuffer = 0;
  m_musicSource = 0;

  alcMakeContextCurrent(nullptr);
  alcDestroyContext(m_context);
  alcCloseDevice(m_device);
}

} // namespace

AudioSystemPtr createAudioSystem(FileSystem& fileSystem)
{
  return std::make_unique<AudioSystemImpl>(fileSystem);
}
