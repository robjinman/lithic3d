#include "utils.hpp"
#include "version.hpp"
#include "exception.hpp"
#include <fstream>
#include <map>
#include <random>

namespace
{

std::map<HashedString, std::string>& getHashTable()
{
  static std::map<HashedString, std::string> hashTable;
  return hashTable;
}

}

std::string versionString()
{
  return STR("Minefield " << Minefield_VERSION_MAJOR << "." << Minefield_VERSION_MINOR);
}

HashedString hashString(const std::string& s)
{
  auto hash = std::hash<std::string>{}(s);
  auto& hashTable = getHashTable();

#ifndef NDEBUG
  // Collision check
  auto i = hashTable.find(hash);
  if (i != hashTable.end() && i->second != s) {
    EXCEPTION("Hash collision; '" << s << "' and '" << i->second << "' both hash to " << hash);
  }
#endif
  hashTable.insert({ hash, s });

  return hash;
}

std::string getHashedString(HashedString hash)
{
  return getHashTable().at(hash);
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
