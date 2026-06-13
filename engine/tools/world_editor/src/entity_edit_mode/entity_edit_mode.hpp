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

    // Call once per frame
    virtual void update() = 0;

    // Persist changes to file
    virtual void saveChanges() = 0;

    // This class maintains the following state:
    //   An entity ID (which could be a temporarily instantiated prefab if editing a prefab)
    //   A spatial system AABB
    //   A set of collision system bounding boxes (some entities are aggregates with more than one)

    // Creates a temporary entity from the prefab and sets it to the current entity
    virtual void setActivePrefab(const std::string& name) = 0;

    // The entity ID of the currently instatiated prefab (if in prefab edit mode)
    virtual lithic3d::EntityId instantiatedPrefabId() const = 0;

    virtual void renderBoundingBox(uint32_t index, bool render) = 0;
    virtual void renderAabb(bool render) = 0;

    // Allows the bounding box to be manipulated with the 3D cursor
    virtual void selectBoundingBox(uint32_t index) = 0;

    // Update one of the current bounding boxes
    virtual void updateBoundingBox(const lithic3d::BoundingBox& box, uint32_t index) = 0;

    // Update the current AABB
    virtual void updateAabb(const lithic3d::Aabb& aabb) = 0;

    virtual const lithic3d::BoundingBox& getBoundingBox(uint32_t index) const = 0;
    virtual const lithic3d::Aabb& getAabb() const = 0;

    // Apply the cursor transform to the currently selected bounding box
    virtual void applyTransform() = 0;

    // Deselect the bounding box
    virtual void cancelTransform() = 0;

    // Update the current entity's components with the current AABB and bounding boxes
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
