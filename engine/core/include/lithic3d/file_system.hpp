#pragma once

#include <vector>
#include <filesystem>
#include <memory>

namespace lithic3d
{

class Directory
{
  public:
    class IteratorImpl
    {
      public:
        virtual std::filesystem::path get() const = 0;
        virtual void next() = 0;
        virtual bool operator==(const IteratorImpl& rhs) const = 0;

        virtual ~IteratorImpl() {}
    };

    class Iterator
    {
      public:
        Iterator(std::unique_ptr<IteratorImpl> impl)
          : m_impl(std::move(impl)) {}

        std::filesystem::path operator*() { return m_impl->get(); }
        bool operator==(const Iterator& rhs) const { return *m_impl == *rhs.m_impl; }
        bool operator!=(const Iterator& rhs) const { return !(*m_impl == *rhs.m_impl); }
        Iterator& operator++() { m_impl->next(); return *this; }

      private:
        std::unique_ptr<IteratorImpl> m_impl;
    };

    using IteratorPtr = std::unique_ptr<Iterator>;

    virtual Iterator begin() const = 0;
    virtual Iterator end() const = 0;

    virtual std::shared_ptr<Directory> subdirectory(const std::filesystem::path& path) const = 0;
    virtual std::vector<char> readFile(const std::filesystem::path& path) const = 0;
    virtual void writeFile(const std::filesystem::path& path, const char* data, size_t size) = 0;
    virtual bool fileExists(const std::filesystem::path& path) const = 0;

    virtual ~Directory() = default;
};

using DirectoryPtr = std::shared_ptr<Directory>;

struct FilePath
{
  DirectoryPtr directory = nullptr;
  std::filesystem::path subpath;

  inline bool exists() const
  {
    return directory->fileExists(subpath);
  }

  inline std::vector<char> read() const
  {
    return directory->readFile(subpath);
  }

  inline void write(const char* data, size_t size)
  {
    directory->writeFile(subpath, data, size);
  }
};

class FileSystem
{
  public:
    virtual DirectoryPtr appDataDirectory() const = 0;
    virtual DirectoryPtr userDataDirectory() const = 0;
    virtual DirectoryPtr directory(const std::filesystem::path& path) const = 0;

    virtual ~FileSystem() = default;
};

using FileSystemPtr = std::unique_ptr<FileSystem>;

} // namespace lithic3d
