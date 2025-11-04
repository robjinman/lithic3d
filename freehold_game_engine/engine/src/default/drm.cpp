#include <fge/drm.hpp>
#include <fge/file_system.hpp>
#include <fge/logger.hpp>
#include <fge/utils.hpp>
#include <fge/exception.hpp>
#include <fge/utils.hpp>
#include <fge/strings.hpp>
#include <openssl/sha.h>
#include <fstream>
#include <cstring>
#include <chrono>

namespace fs = std::filesystem;

namespace fge
{
namespace
{

const std::string activationFile = "activation.dat";
const uint32_t keyLength = 8;

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

class DrmImpl : public Drm
{
  public:
    DrmImpl(const std::string& productName, FileSystem& fileSystem, Logger& logger);

    bool isActivated() const override;
    bool activate(const std::string& key) override;

  private:
    std::string m_productName;
    FileSystem& m_fileSystem;
    Logger& m_logger;

    void writeActivationFile();
    std::string secret(int dayOffset) const;
    std::string getSystemId() const;
};

DrmImpl::DrmImpl(const std::string& productName, FileSystem& fileSystem, Logger& logger)
  : m_productName(productName)
  , m_fileSystem(fileSystem)
  , m_logger(logger)
{
}

bool DrmImpl::isActivated() const
{
  if (!m_fileSystem.userDataFileExists(activationFile)) {
    return false;
  }

  auto systemId = getSystemId();
  ASSERT(!systemId.empty(), "Error checking product activation status");

  std::stringstream ss;
  ss << m_productName;
  ss << "ByFreeholdAppsLtd";
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
  // Accept any key generated within the last few days or for the next few days
  const int offset = -5;
  const int numKeys = 10;

  for (int i = 0; i < numKeys; ++i) {
    if (sha256(secret(i + offset)) == key) {
      writeActivationFile();
      return true;
    }
  }

  return false;
}

std::string DrmImpl::secret(int dayOffset) const
{
  auto now = std::chrono::system_clock::now() + std::chrono::days{dayOffset};
  auto today = std::chrono::floor<std::chrono::days>(now);
  std::chrono::year_month_day ymd{today};

  std::stringstream ss;
  ss << m_productName;
  ss << "ByFreeholdAppsLtd";
  ss << static_cast<int>(ymd.year());
  ss << static_cast<unsigned>(ymd.month());
  ss << static_cast<unsigned>(ymd.day());

  DBG_LOG(m_logger, STR("Secret: " << ss.str()));

  return ss.str();
}

void DrmImpl::writeActivationFile()
{
  auto systemId = getSystemId();
  ASSERT(!systemId.empty(), "Error activating product");

  std::stringstream ss;
  ss << m_productName;
  ss << "ByFreeholdAppsLtd";
  ss << systemId;

  auto hash = sha256(ss.str());

  m_fileSystem.writeUserDataFile(activationFile, hash.c_str(), hash.size());
}

#ifdef __WIN32

// TODO

#elif defined(__APPLE__)

std::string getSystemId()
{
  // TODO
  return "aaaaaaaaaabbbbbbbbbbcccccccccc12";
}

#else

std::string DrmImpl::getSystemId() const
{
  DBG_LOG(m_logger, "Retrieving system ID");

  std::string macAddress;
  for (auto const& entry : std::filesystem::directory_iterator{"/sys/class/net"}) {
    auto path = entry.path();
    std::string fileName = path.filename();

    if (fileName.starts_with("enp") || fileName.starts_with("eth")) {
      auto data = readBinaryFile(path / "address");
      macAddress.assign(data.begin(), data.end());
    }
  }

  if (!macAddress.empty()) {
    DBG_LOG(m_logger, STR("Found MAC address: " << macAddress));
    return macAddress;
  }

  DBG_LOG(m_logger, "No suitable MAC address found. Falling back to machine-id");

  std::ifstream stream{"/etc/machine-id"};
  std::string id;
  stream >> id;

  DBG_LOG(m_logger, STR("Using machine ID: " << id));

  return id;
}

#endif

} // namespace

DrmPtr createDrm(const std::string& productName, FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<DrmImpl>(productName, fileSystem, logger);
}

} // namespace fge
