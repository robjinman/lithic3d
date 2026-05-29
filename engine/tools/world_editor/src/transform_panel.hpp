#pragma once

#include <lithic3d/math.hpp>
#include <memory>

class wxWindow;

// Read-only display of Mat4x4f

class TransformPanel
{
  public:
    virtual wxWindow* getWxPtr() = 0;
    virtual void setTransform(const lithic3d::Mat4x4f& transform) = 0;
    virtual const lithic3d::Mat4x4f& getTransform() const = 0;

    virtual ~TransformPanel() = default;
};

using TransformPanelPtr = std::unique_ptr<TransformPanel>;

TransformPanelPtr createTransformPanel(wxWindow* parent);
