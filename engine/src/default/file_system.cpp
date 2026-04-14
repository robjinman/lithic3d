#include <lithic3d/file_system.hpp>
#include <lithic3d/exception.hpp>
#include <lithic3d/utils.hpp>
#include <lithic3d/platform_paths.hpp>
#include <fstream>

namespace fs = std::filesystem;

namespace lithic3d
{
namespace
{

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
    explicit DefaultDirectoryImpl(const fs::path& path);

    Directory::Iterator begin() const override;
    Directory::Iterator end() const override;

    DirectoryPtr subdirectory(const fs::path& path) const override;
    std::vector<char> readFile(const fs::path& path) const override;
    void writeFile(const fs::path& path, const char* data, size_t size) override;
    bool fileExists(const fs::path& path) const override;

  private:
    fs::path m_path;
};

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

DefaultDirectoryImpl::DefaultDirectoryImpl(const fs::path& path)
  : m_path(path)
{
}

DirectoryPtr DefaultDirectoryImpl::subdirectory(const fs::path& path) const
{
  return std::make_unique<DefaultDirectoryImpl>(m_path / path);
}

std::vector<char> DefaultDirectoryImpl::readFile(const fs::path& path) const
{
  return readBinaryFile((m_path / path).string());
}

void DefaultDirectoryImpl::writeFile(const fs::path& path, const char* data, size_t size)
{
  writeBinaryFile(m_path / path, data, size);
}

bool DefaultDirectoryImpl::fileExists(const fs::path& path) const
{
  return fs::exists(m_path / path);
}

class DefaultFileSystem : public FileSystem
{
  public:
    explicit DefaultFileSystem(PlatformPathsPtr platformPaths);

    DirectoryPtr appDataDirectory() const override;
    DirectoryPtr userDataDirectory() const override;
    DirectoryPtr directory(const fs::path& path) const override;

  private:
    const PlatformPathsPtr m_paths;
};

DefaultFileSystem::DefaultFileSystem(PlatformPathsPtr platformPaths)
  : m_paths(std::move(platformPaths))
{
}

DirectoryPtr DefaultFileSystem::appDataDirectory() const
{
  return std::make_unique<DefaultDirectoryImpl>(m_paths->appData());
}

DirectoryPtr DefaultFileSystem::userDataDirectory() const
{
  return std::make_unique<DefaultDirectoryImpl>(m_paths->userData());
}

DirectoryPtr DefaultFileSystem::directory(const fs::path& path) const
{
  return std::make_unique<DefaultDirectoryImpl>(path);
}

} // namespace

FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths)
{
  return std::make_unique<DefaultFileSystem>(std::move(platformPaths));
}

} // namespace lithic3d
