#include "strings.hpp"
#include "exception.hpp"
#include <map>

namespace
{

std::map<HashedString, std::string>& getHashTable()
{
  static std::map<HashedString, std::string> hashTable;
  return hashTable;
}

}

// TODO: Either do collision check on release builds too (and handle appropriately) or use hash
// function that's guaranteed to produce the same hashes on all platforms
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
