#include <lithic3d/file_system.hpp>
#include <lithic3d/exception.hpp>
#include <lithic3d/utils.hpp>
#include <android/asset_manager.h>

namespace fs = std::filesystem;

namespace lithic3d
{
namespace
{

std::vector<char> readAssetFile(AAssetManager& assetManager, const fs::path& path)
{
  AAsset* asset = AAssetManager_open(&assetManager, path.c_str(), AASSET_MODE_BUFFER);
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
    AndroidDirectoryImpl(AAssetManager& assetManager, const fs::path& path, bool assetDir);

    Directory::Iterator begin() const override;
    Directory::Iterator end() const override;

    std::shared_ptr<Directory> subdirectory(const fs::path& path) const override;
    std::vector<char> readFile(const fs::path& path) const override;
    void writeFile(const fs::path& path, const char* data, size_t size) override;
    bool fileExists(const fs::path& path) const override;

  private:
    AAssetManager& m_assetManager;
    fs::path m_path;
    bool m_isAssetDir;
};

AndroidDirectoryImpl::AndroidDirectoryImpl(AAssetManager& assetManager, const fs::path& path,
  bool assetDir)
  : m_assetManager(assetManager)
  , m_path(path)
  , m_isAssetDir(assetDir)
{
}

Directory::Iterator AndroidDirectoryImpl::begin() const
{
  ASSERT(m_isAssetDir, "Directory iteration only supported on asset directories");

  auto assetDir = AAssetManager_openDir(&m_assetManager, m_path.c_str());
  return Directory::Iterator{std::make_unique<AndroidIteratorImpl>(m_path, assetDir)};
}

Directory::Iterator AndroidDirectoryImpl::end() const
{
  ASSERT(m_isAssetDir, "Directory iteration only supported on asset directories");

  return Directory::Iterator{std::make_unique<AndroidIteratorImpl>(m_path, nullptr)};
}

DirectoryPtr AndroidDirectoryImpl::subdirectory(const fs::path& path) const
{
  return std::make_shared<AndroidDirectoryImpl>(m_assetManager, m_path / path, m_isAssetDir);
}

std::vector<char> AndroidDirectoryImpl::readFile(const fs::path& path) const
{
  if (m_isAssetDir) {
    return readAssetFile(m_assetManager, m_path / path);
  }
  else {
    return readBinaryFile(m_path / path);
  }
}

void AndroidDirectoryImpl::writeFile(const fs::path& path, const char* data, size_t size)
{
  ASSERT(!m_isAssetDir, "Cannot write to app data files");

  writeBinaryFile(m_path / path, data, size);
}

bool AndroidDirectoryImpl::fileExists(const fs::path& path) const
{
  ASSERT(!m_isAssetDir, "Not implemented for user data files");
  return fs::exists(m_path / path);
}

class AndroidFileSystem : public FileSystem
{
  public:
    AndroidFileSystem(const fs::path& userDataPath, AAssetManager& assetManager);

    DirectoryPtr appDataDirectory() const override;
    DirectoryPtr userDataDirectory() const override;
    DirectoryPtr directory(const fs::path& path) const override;

  private:
    fs::path m_userDataPath;
    AAssetManager& m_assetManager;
};

AndroidFileSystem::AndroidFileSystem(const fs::path& userDataPath, AAssetManager& assetManager)
  : m_userDataPath(userDataPath)
  , m_assetManager(assetManager)
{
}

DirectoryPtr AndroidFileSystem::appDataDirectory() const
{
  return std::make_unique<AndroidDirectoryImpl>(m_assetManager, "", true);
}

DirectoryPtr AndroidFileSystem::userDataDirectory() const
{
  return std::make_unique<AndroidDirectoryImpl>(m_assetManager, m_userDataPath, false);
}

DirectoryPtr AndroidFileSystem::directory(const fs::path&) const
{
  EXCEPTION("Not implemented");
}

} // namespace

FileSystemPtr createAndroidFileSystem(const fs::path& userDataPath, AAssetManager& assetManager)
{
  return std::make_unique<AndroidFileSystem>(userDataPath, assetManager);
}

} // namespace lithic3d
