#pragma once

#include <memory>

class PrefabEditMode
{
  public:
    virtual void activate() = 0;
    virtual void deactivate() = 0;

    virtual ~PrefabEditMode() = default;
};

using PrefabEditModePtr = std::unique_ptr<PrefabEditMode>;

class EditorCore;

PrefabEditModePtr createPrefabEditMode(EditorCore& core);
