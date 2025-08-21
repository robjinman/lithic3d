#include "utils.hpp"
#include "version.hpp"
#include "exception.hpp"
#include <fstream>
#include <map>

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
  return STR("Nova " << Nova_VERSION_MAJOR << "." << Nova_VERSION_MINOR);
}

std::vector<char> readBinaryFile(const std::string& filename)
{
  std::ifstream stream(filename, std::ios::ate | std::ios::binary);

  if (!stream.is_open()) {
    EXCEPTION("Failed to open file " << filename);
  }

  size_t fileSize = stream.tellg();
  std::vector<char> bytes(fileSize);

  stream.seekg(0);
  stream.read(bytes.data(), fileSize);

  return bytes;
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
