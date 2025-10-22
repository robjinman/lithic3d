#include "utils.hpp"
#include "version.hpp"
#include "exception.hpp"
#include "platform.hpp"
#include "strings.hpp"
#include <fstream>
#include <random>

std::string getVersionString()
{
  static std::string s = []() {
    return STR(Minefield_VERSION_MAJOR << "." << Minefield_VERSION_MINOR
      << "-" << PLATFORM_NAME
#ifdef DRM
      << "-drm"
#endif
      << "");
  }();

  return s;
}

std::string getBuildId()
{
  return BUILD_ID;
}

uint32_t getVersionMajor()
{
  return Minefield_VERSION_MAJOR;
}

uint32_t getVersionMinor()
{
  return Minefield_VERSION_MINOR;
}

int randomInt()
{
  std::random_device device;
  std::mt19937 generator(device());
  std::uniform_int_distribution distribution;

  return distribution(generator);
}

std::vector<char> readBinaryFile(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::ate | std::ios::binary);

  if (!stream.is_open()) {
    EXCEPTION("Failed to open file " << path);
  }

  size_t fileSize = stream.tellg();
  std::vector<char> bytes(fileSize);

  stream.seekg(0);
  stream.read(bytes.data(), fileSize);

  return bytes;
}

void writeBinaryFile(const std::filesystem::path& path, const char* data, size_t size)
{
  std::filesystem::create_directories(path.parent_path());

  std::ofstream stream(path, std::ios::binary);

  if (!stream.is_open()) {
    EXCEPTION("Failed to open file " << path);
  }

  stream.write(data, size);
}
