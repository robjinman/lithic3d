#include "file_system.hpp"
#include "exception.hpp"
#include "utils.hpp"
#include "platform_paths.hpp"
#include <fstream>

namespace fs = std::filesystem;

namespace
{

std::vector<char> readBinaryFile(const fs::path& path)
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

void writeBinaryFile(const fs::path& path, const char* data, size_t size)
{
  fs::create_directories(path.parent_path());

  std::ofstream stream(path, std::ios::binary);

  if (!stream.is_open()) {
    EXCEPTION("Failed to open file " << path);
  }

  stream.write(data, size);
}

class DefaultIteratorImpl : public Directory::IteratorImpl
{
  public:
    DefaultIteratorImpl(fs::directory_iterator iterator);

    fs::path get() const override;
    void next() override;
    bool operator==(const IteratorImpl& rhs) const override;

  private:
    fs::directory_iterator m_iterator;
};

DefaultIteratorImpl::DefaultIteratorImpl(fs::directory_iterator iterator)
  : m_iterator(iterator)
{
}

fs::path DefaultIteratorImpl::get() const
{
  return *m_iterator;
}

void DefaultIteratorImpl::next()
{
  ++m_iterator;
}

bool DefaultIteratorImpl::operator==(const IteratorImpl& rhs) const
{
  return m_iterator == dynamic_cast<const DefaultIteratorImpl&>(rhs).m_iterator;
}

class DefaultDirectoryImpl : public Directory
{
  public:
    DefaultDirectoryImpl(const fs::path& path);

    Directory::Iterator begin() const override;
    Directory::Iterator end() const override;

  private:
    fs::path m_path;
};

DefaultDirectoryImpl::DefaultDirectoryImpl(const fs::path& path)
  : m_path(path)
{
}

Directory::Iterator DefaultDirectoryImpl::begin() const
{
  auto i = fs::directory_iterator{m_path};
  return Directory::Iterator{std::make_unique<DefaultIteratorImpl>(i)};
}

Directory::Iterator DefaultDirectoryImpl::end() const
{
  auto i = fs::end(fs::directory_iterator{m_path});
  return Directory::Iterator{std::make_unique<DefaultIteratorImpl>(i)};
}

class DefaultFileSystem : public FileSystem
{
  public:
    DefaultFileSystem(const PlatformPaths& platformPaths);

    std::vector<char> readAppDataFile(const fs::path& path) const override;
    DirectoryPtr appDataDirectory(const fs::path& path) const override;

    bool userDataFileExists(const std::filesystem::path& path) const override;
    std::vector<char> readUserDataFile(const fs::path& path) const override;
    void writeUserDataFile(const fs::path& path, const char* data, size_t size) override;

  private:
    const PlatformPaths& m_paths;
};

DefaultFileSystem::DefaultFileSystem(const PlatformPaths& platformPaths)
  : m_paths(platformPaths)
{
}

std::vector<char> DefaultFileSystem::readAppDataFile(const fs::path& path) const
{
  return readBinaryFile((m_paths.appData() / path).string());
}

DirectoryPtr DefaultFileSystem::appDataDirectory(const fs::path& path) const
{
  return std::make_unique<DefaultDirectoryImpl>(m_paths.appData() / path);
}

bool DefaultFileSystem::userDataFileExists(const std::filesystem::path& path) const
{
  return std::filesystem::exists(m_paths.userData() / path);
}

std::vector<char> DefaultFileSystem::readUserDataFile(const fs::path& path) const
{
  return readBinaryFile((m_paths.userData() / path).string());
}

void DefaultFileSystem::writeUserDataFile(const fs::path& path, const char* data, size_t size)
{
  writeBinaryFile(m_paths.userData() / path, data, size);
}

} // namespace

FileSystemPtr createDefaultFileSystem(const PlatformPaths& platformPaths)
{
  return std::make_unique<DefaultFileSystem>(platformPaths);
}
