#include "mode_ui.hpp"

namespace
{

class PrefabEditModeUi : public ModeUi
{
  public:
    void activate() override;
    void deactivate() override;

    void update() override;

    void onKeyDown(lithic3d::KeyboardKey key) override;
    void onKeyUp(lithic3d::KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;

    void saveChanges() override;
};

void PrefabEditModeUi::activate()
{
}

void PrefabEditModeUi::deactivate()
{
}

void PrefabEditModeUi::update()
{
}

void PrefabEditModeUi::onKeyDown(lithic3d::KeyboardKey key)
{
}

void PrefabEditModeUi::onKeyUp(lithic3d::KeyboardKey key)
{
}

void PrefabEditModeUi::onMouseLeftBtnDown()
{
}

void PrefabEditModeUi::onMouseLeftBtnUp()
{
}

void PrefabEditModeUi::onMouseMove(float x, float y)
{
}

void PrefabEditModeUi::saveChanges()
{
}

} // namespace

ModeUiPtr createPrefabEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel,
  EditorCore& editorCore)
{
  return std::make_unique<PrefabEditModeUi>();
}
