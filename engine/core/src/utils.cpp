#include "version.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/platform.hpp"
#include "lithic3d/strings.hpp"
#include <fstream>
#include <random>

namespace lithic3d
{

std::string getVersionString()
{
  static std::string s = []() {
    return STR(Lithic3D_VERSION_MAJOR << "." << Lithic3D_VERSION_MINOR << "-" << PLATFORM_NAME);
  }();

  return s;
}

//std::string getBuildId()
//{
//  return BUILD_ID;
//}

uint32_t getVersionMajor()
{
  return Lithic3D_VERSION_MAJOR;
}

uint32_t getVersionMinor()
{
  return Lithic3D_VERSION_MINOR;
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

} // namespace lithic3d
