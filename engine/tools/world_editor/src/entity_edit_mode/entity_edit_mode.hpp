#pragma once

#include <lithic3d/input.hpp>
#include <lithic3d/entity_id.hpp>
#include <lithic3d/sys_collision.hpp>
#include <memory>

enum class ShapeType
{
  Box,
  Cylinder,
  Ovoid
};

struct Shape
{
  explicit Shape(ShapeType type)
    : type(type) {}

  ShapeType type;

  virtual std::unique_ptr<Shape> clone() const = 0;

  virtual ~Shape() = default;
};

using ShapePtr = std::unique_ptr<Shape>;

template<typename T>
struct ShapeWrapper : public Shape
{
  ShapeWrapper(const T& shape, ShapeType type)
    : Shape(type)
    , shape(shape) {}

  std::unique_ptr<Shape> clone() const override
  {
    return std::make_unique<ShapeWrapper<T>>(shape, type);
  }

  T shape;
};

inline auto createBoxShape(const lithic3d::BoundingBox& shape) {
  return ShapeWrapper<lithic3d::BoundingBox>{shape, ShapeType::Box};
}
inline auto createCylinderShape(const lithic3d::Cylinder& shape) {
  return ShapeWrapper<lithic3d::Cylinder>{shape, ShapeType::Cylinder};
}
inline auto createOvoidShape(const lithic3d::Ovoid& shape) {
  return ShapeWrapper<lithic3d::Ovoid>{shape, ShapeType::Ovoid};
}

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
    //   A set of collision system volumes (referred to as shapes). Some entities are aggregates
    //     with more than one

    // Creates a temporary entity from the prefab and sets it to the current entity
    virtual void setActivePrefab(const std::string& name) = 0;

    // The entity ID of the currently instatiated prefab (if in prefab edit mode)
    virtual lithic3d::EntityId instantiatedPrefabId() const = 0;

    virtual void renderShape(uint32_t index, bool render) = 0;
    virtual void renderAabb(bool render) = 0;

    // Allows the shape to be manipulated with the 3D cursor
    virtual void selectShape(uint32_t index) = 0;

    // Update one of the current shapes
    virtual void updateShape(const Shape& shape, uint32_t index) = 0;

    virtual void addShape(const Shape& shape) = 0;

    // Update the current AABB
    virtual void updateAabb(const lithic3d::Aabb& aabb) = 0;

    virtual const lithic3d::Aabb& getAabb() const = 0;
    virtual const Shape& getShape(uint32_t index) const = 0;

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
