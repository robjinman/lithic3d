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

class DefaultDirectoryImpl : public Directory
{
  public:
    explicit DefaultDirectoryImpl(const fs::path& path);

    DirectoryPtr subdirectory(const std::filesystem::path& path) const override;
    std::vector<char> readFile(const std::filesystem::path& path) const override;
    void writeFile(const std::filesystem::path& path, const char* data, size_t size) override;
    bool fileExists(const std::filesystem::path& path) const override;

  private:
    fs::path m_path;
};

DefaultDirectoryImpl::DefaultDirectoryImpl(const fs::path& path)
  : m_path(path)
{
}

DirectoryPtr DefaultDirectoryImpl::subdirectory(const std::filesystem::path& path) const
{
  return std::make_unique<DefaultDirectoryImpl>(m_path / path);
}

std::vector<char> DefaultDirectoryImpl::readFile(const std::filesystem::path& path) const
{
  return readBinaryFile((m_path / path).string());
}

void DefaultDirectoryImpl::writeFile(const std::filesystem::path& path, const char* data,
  size_t size)
{
  writeBinaryFile(m_path / path, data, size);
}

bool DefaultDirectoryImpl::fileExists(const std::filesystem::path& path) const
{
  return std::filesystem::exists(m_path / path);
}

class DefaultFileSystem : public FileSystem
{
  public:
    explicit DefaultFileSystem(PlatformPathsPtr platformPaths);

    DirectoryPtr appDataDirectory() const override;
    DirectoryPtr userDataDirectory() const override;
    DirectoryPtr directory(const std::filesystem::path& path) const override;

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

DirectoryPtr DefaultFileSystem::directory(const std::filesystem::path& path) const
{
  return std::make_unique<DefaultDirectoryImpl>(path);
}

} // namespace

FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths)
{
  return std::make_unique<DefaultFileSystem>(std::move(platformPaths));
}

} // namespace lithic3d
