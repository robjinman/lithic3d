#include "drm.hpp"
#include "file_system.hpp"

namespace
{

class DrmImpl : public Drm
{
  public:
    DrmImpl(FileSystem& fileSystem);

    bool isActivated() const override;
    bool activate(const std::string& key) override;

  private:
    FileSystem& m_fileSystem;
};

DrmImpl::DrmImpl(FileSystem& fileSystem)
  : m_fileSystem(fileSystem)
{
}

bool DrmImpl::isActivated() const
{
  // TODO
  return false;
}

bool DrmImpl::activate(const std::string& key)
{
  // TODO
  return true;
}

} // namespace

DrmPtr createDrm(FileSystem& fileSystem)
{
  return std::make_unique<DrmImpl>(fileSystem);
}
