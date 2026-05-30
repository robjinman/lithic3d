#pragma once

#include <lithic3d/input.hpp>
#include <lithic3d/entity_id.hpp>
#include <lithic3d/sys_collision.hpp>
#include <memory>

class EntityEditMode
{
  public:
    virtual void activate() = 0;
    virtual void deactivate() = 0;
    virtual void update() = 0;
    virtual void saveChanges() = 0;

    virtual void setActivePrefab(const std::string& name) = 0;
    virtual lithic3d::EntityId instantiatedPrefabId() const = 0;

    virtual void renderBoundingBox(bool render) = 0;
    virtual void renderAabb(bool render) = 0;

    virtual void selectBoundingBox() = 0;

    virtual void updateBoundingBox(const lithic3d::BoundingBox& box) = 0;
    virtual void updateAabb(const lithic3d::Aabb& aabb) = 0;

    virtual const lithic3d::BoundingBox& getBoundingBox() const = 0;
    virtual const lithic3d::Aabb& getAabb() const = 0;

    virtual void applyTransform() = 0;
    virtual void cancelTransform() = 0;

    virtual void applyChangesToEntity() = 0;

    virtual void onKeyDown(lithic3d::KeyboardKey key) = 0;
    virtual void onKeyUp(lithic3d::KeyboardKey key) = 0;
    virtual void onMouseLeftBtnDown() = 0;
    virtual void onMouseLeftBtnUp() = 0;
    virtual void onMouseMove(float x, float y) = 0;

    virtual ~EntityEditMode() = default;
};

using EntityEditModePtr = std::unique_ptr<EntityEditMode>;

class EditorCore;

EntityEditModePtr createEntityEditMode(EditorCore& core);
