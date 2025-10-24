#include "file_system.hpp"
#include "exception.hpp"
#include "utils.hpp"
#include <android/asset_manager.h>

namespace fs = std::filesystem;

namespace fge
{
namespace
{

class AndroidIteratorImpl : public Directory::IteratorImpl
{
  public:
    AndroidIteratorImpl(const fs::path& root, AAssetDir* assetDir);

    fs::path get() const override;
    void next() override;
    bool operator==(const IteratorImpl& rhs) const override;

    ~AndroidIteratorImpl() override;

  private:
    AAssetDir* m_assetDir;
    fs::path m_root;
    fs::path m_path;
};

AndroidIteratorImpl::AndroidIteratorImpl(const fs::path& root, AAssetDir* assetDir)
  : m_assetDir(assetDir)
  , m_root(root)
{
  if (assetDir != nullptr) {
    next();
  }
}

fs::path AndroidIteratorImpl::get() const
{
  return m_path;
}

void AndroidIteratorImpl::next()
{
  ASSERT(m_assetDir != nullptr, "Advanced beyond end iterator");

  const char* path = AAssetDir_getNextFileName(m_assetDir);
  if (path != nullptr) {
    m_path = m_root / path;
  }
  else if (m_assetDir != nullptr) {
    AAssetDir_close(m_assetDir);
    m_assetDir = nullptr;
    m_path = fs::path{};
  }
}

bool AndroidIteratorImpl::operator==(const IteratorImpl& rhs) const
{
  return m_path == dynamic_cast<const AndroidIteratorImpl&>(rhs).m_path;
}

AndroidIteratorImpl::~AndroidIteratorImpl()
{
  if (m_assetDir != nullptr) {
    AAssetDir_close(m_assetDir);
  }
}

class AndroidDirectoryImpl : public Directory
{
  public:
    AndroidDirectoryImpl(AAssetManager& assetManager, const fs::path& path);

    Directory::Iterator begin() const override;
    Directory::Iterator end() const override;

  private:
    AAssetManager& m_assetManager;
    fs::path m_path;
};

AndroidDirectoryImpl::AndroidDirectoryImpl(AAssetManager& assetManager, const fs::path& path)
  : m_assetManager(assetManager)
  , m_path(path)
{
}

Directory::Iterator AndroidDirectoryImpl::begin() const
{
  auto assetDir = AAssetManager_openDir(&m_assetManager, m_path.c_str());
  return Directory::Iterator{std::make_unique<AndroidIteratorImpl>(m_path, assetDir)};
}

Directory::Iterator AndroidDirectoryImpl::end() const
{
  return Directory::Iterator{std::make_unique<AndroidIteratorImpl>(m_path, nullptr)};
}

class AndroidFileSystem : public FileSystem
{
  public:
    AndroidFileSystem(const fs::path& userDataPath, AAssetManager& assetManager);

    std::vector<char> readAppDataFile(const fs::path& path) const override;
    DirectoryPtr appDataDirectory(const fs::path& path) const override;

    bool userDataFileExists(const fs::path& path) const override;
    std::vector<char> readUserDataFile(const fs::path& path) const override;
    void writeUserDataFile(const fs::path& path, const char* data,
      size_t size) override;

  private:
    fs::path m_userDataPath;
    AAssetManager& m_assetManager;
};

AndroidFileSystem::AndroidFileSystem(const fs::path& userDataPath, AAssetManager& assetManager)
  : m_userDataPath(userDataPath)
  , m_assetManager(assetManager)
{
}

std::vector<char> AndroidFileSystem::readAppDataFile(const fs::path& path) const
{
  AAsset* asset = AAssetManager_open(&m_assetManager, path.c_str(), AASSET_MODE_BUFFER);
  ASSERT(asset != nullptr, "Error opening asset '" << path << "'");

  size_t assetLength = AAsset_getLength(asset);
  std::vector<char> data(assetLength);

  size_t read = 0;
  int result = 0;
  do {
    result = AAsset_read(asset, data.data() + read, assetLength - read);
    read += result;
  } while (result > 0);

  return data;
}

DirectoryPtr AndroidFileSystem::appDataDirectory(const fs::path& path) const
{
  return std::make_unique<AndroidDirectoryImpl>(m_assetManager, path);
}

bool AndroidFileSystem::userDataFileExists(const std::filesystem::path& path) const
{
  return std::filesystem::exists(m_userDataPath / path);
}

std::vector<char> AndroidFileSystem::readUserDataFile(const fs::path& path) const
{
  return readBinaryFile((m_userDataPath / path).string());
}

void AndroidFileSystem::writeUserDataFile(const fs::path& path, const char* data, size_t size)
{
  writeBinaryFile(m_userDataPath / path, data, size);
}

} // namespace

FileSystemPtr createAndroidFileSystem(const fs::path& userDataPath, AAssetManager& assetManager)
{
  return std::make_unique<AndroidFileSystem>(userDataPath, assetManager);
}

} // namespace fge
