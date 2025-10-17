#include "drm.hpp"
#include "file_system.hpp"
#include "utils.hpp"
#include "exception.hpp"
#include <openssl/sha.h>
#include <fstream>
#include <cstring>
#include <chrono>

namespace fs = std::filesystem;

namespace
{

const std::string activationFile = "activation.dat";
const uint32_t keyLength = 8;

#ifdef __WIN32

// TODO

#elif defined(__APPLE__)

std::string getSystemId()
{
  // TODO
  return "aaaaaaaaaabbbbbbbbbbcccccccccc12";
}

#else

std::string getSystemId()
{
  std::ifstream stream{"/etc/machine-id"};
  std::string id;
  stream >> id;

  return id;
}

#endif

// JavaScript equivalent:
// crypto.createHash('sha256').update(input).digest('hex').slice(0, keyLength).toUpperCase()
std::string sha256(const std::string& input)
{
  unsigned char hash[SHA256_DIGEST_LENGTH];

  SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

  std::stringstream ss;
  for (auto c : hash) {
    ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }

  return ss.str().substr(0, keyLength);
}

std::string secret(unsigned plusDays)
{
  auto now = std::chrono::system_clock::now();
  auto today = std::chrono::floor<std::chrono::days>(now);
  std::chrono::year_month_day ymd{today};

  std::stringstream ss;
  ss << "MinefieldByFreeholdAppsLtd";
  ss << static_cast<int>(ymd.year());
  ss << static_cast<unsigned>(ymd.month());
  ss << static_cast<unsigned>(ymd.day()) + plusDays;

  return ss.str();
}

class DrmImpl : public Drm
{
  public:
    DrmImpl(FileSystem& fileSystem);

    bool isActivated() const override;
    bool activate(const std::string& key) override;

  private:
    FileSystem& m_fileSystem;

    void writeActivationFile();
};

DrmImpl::DrmImpl(FileSystem& fileSystem)
  : m_fileSystem(fileSystem)
{
}

bool DrmImpl::isActivated() const
{
  if (!m_fileSystem.userDataFileExists(activationFile)) {
    return false;
  }

  auto systemId = getSystemId();
  ASSERT(systemId.length() == 32, "Error checking product activation status");

  std::stringstream ss;
  ss << "MinefieldByFreeholdAppsLtd";
  ss << systemId;

  auto expected = sha256(ss.str());
  auto actual = m_fileSystem.readUserDataFile(activationFile);

  if (expected.size() != actual.size()) {
    return false;
  }

  return strncmp(actual.data(), expected.c_str(), expected.size()) == 0;
}

bool DrmImpl::activate(const std::string& key)
{
  // Compare against keys for 5 days starting from yesterday
  const int offset = -1;
  const int numKeys = 5;

  for (int i = 0; i < numKeys; ++i) {
    if (sha256(secret(i + offset)) == key) {
      writeActivationFile();
      return true;
    }
  }

  return false;
}

void DrmImpl::writeActivationFile()
{
  auto systemId = getSystemId();
  ASSERT(systemId.length() == 32, "Error activating product");

  std::stringstream ss;
  ss << "MinefieldByFreeholdAppsLtd";
  ss << systemId;

  auto hash = sha256(ss.str());

  m_fileSystem.writeUserDataFile(activationFile, hash.c_str(), hash.size());
}

} // namespace

DrmPtr createDrm(FileSystem& fileSystem)
{
  return std::make_unique<DrmImpl>(fileSystem);
}
