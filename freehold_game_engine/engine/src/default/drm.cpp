#include <fge/drm.hpp>
#include <fge/file_system.hpp>
#include <fge/logger.hpp>
#include <fge/utils.hpp>
#include <fge/exception.hpp>
#include <fge/utils.hpp>
#include <fge/strings.hpp>
#include <fge/platform.hpp>
#include <openssl/sha.h>
#include <fstream>
#include <cstring>
#include <chrono>
#ifdef PLATFORM_OSX
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <sys/socket.h>
#include <IOKit/IOKitLib.h>
#endif

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

#ifdef PLATFORM_OSX
    std::string getMacAddress() const;
    std::string getPlatformUuid() const;
#endif
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

#ifdef PLATFORM_WINDOWS

std::string DrmImpl::getSystemId() const
{
  // TODO
  return "abc123";
}

#elif defined(PLATFORM_OSX)

std::string DrmImpl::getPlatformUuid() const
{
  io_registry_entry_t root = IORegistryEntryFromPath(kIOMainPortDefault, "IOService:/");
  CFStringRef id = reinterpret_cast<CFStringRef>(IORegistryEntryCreateCFProperty(root,
    CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0));
  IOObjectRelease(root);

  char buffer[256];
  CFStringGetCString(id, buffer, sizeof(buffer), kCFStringEncodingMacRoman);
  CFRelease(id);

  return std::string{buffer};
}

std::string DrmImpl::getMacAddress() const
{
  ifaddrs* interfaceAddrs = nullptr;
  if (getifaddrs(&interfaceAddrs) != 0) {
    return "";
  }

  std::string macAddress;
  for (auto* addr = interfaceAddrs; addr != nullptr; addr = addr->ifa_next) {
    if (!addr->ifa_addr || addr->ifa_addr->sa_family != AF_LINK) {
      continue;
    }

    sockaddr_dl* socketAddr = reinterpret_cast<sockaddr_dl*>(addr->ifa_addr);
    auto base = reinterpret_cast<unsigned char*>(LLADDR(socketAddr));
    if (socketAddr->sdl_alen == 6 && strncmp(addr->ifa_name, "en", 2) == 0) {
      char buffer[18];
      snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x", base[0], base[1], base[2],
        base[3], base[4], base[5]);
      macAddress = buffer;
      break;
    }
  }

  freeifaddrs(interfaceAddrs);
  return macAddress;
}

std::string DrmImpl::getSystemId() const
{
  auto macAddress = getMacAddress();

  if (!macAddress.empty()) {
    DBG_LOG(m_logger, STR("Found MAC address: " << macAddress));
    return macAddress;
  }

  DBG_LOG(m_logger, "No suitable MAC address found. Falling back to platform UUID");

  auto id = getPlatformUuid();
  DBG_LOG(m_logger, STR("Using platform UUID: " << id));

  return id;
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
