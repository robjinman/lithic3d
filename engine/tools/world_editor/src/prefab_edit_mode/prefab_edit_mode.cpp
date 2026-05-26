#include "prefab_edit_mode/prefab_edit_mode.hpp"
#include "editor_core.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;

namespace
{

class PrefabEditModeImpl : public PrefabEditMode
{
  public:
    PrefabEditModeImpl(EditorCore& core);

    void activate() override;
    void deactivate() override;

  private:
    EditorCore& m_core;
};

PrefabEditModeImpl::PrefabEditModeImpl(EditorCore& core)
  : m_core(core)
{}

void PrefabEditModeImpl::activate()
{
  auto& camera = m_core.engine().ecs().system<SysRender3d>().camera();
  camera.setTransform(lookAt({ 0.f, 0.f, 0.f }, { 0.f, 0.f, -1.f }));
}

void PrefabEditModeImpl::deactivate()
{
  // TODO
}

} // namespace

PrefabEditModePtr createPrefabEditMode(EditorCore& core)
{
  return std::make_unique<PrefabEditModeImpl>(core);
}
