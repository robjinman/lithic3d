#pragma once

#include "renderables.hpp"
#include "work_queue.hpp"

namespace render
{

struct ViewParams
{
  float hFov;
  float vFov;
  float aspectRatio;
  float nearPlane;
  float farPlane;
};

enum class RenderPass
{
  Shadow,
  Main,
  Ssr,
  Overlay
};

struct AddMeshResult : public WorkItemResultValue
{
  AddMeshResult(MeshHandle handle)
    : handle(handle) {}

  MeshHandle handle;
};

struct RemoveMeshResult : public WorkItemResultValue
{
};

class Renderer
{
  public:
    virtual void start() = 0;
    virtual bool isStarted() const = 0;
    virtual double frameRate() const = 0;
    virtual void onResize() = 0;
    virtual const ViewParams& getViewParams() const = 0;
    virtual Vec2i getViewportSize() const = 0;
    virtual void checkError() const = 0;

    // Initialisation
    //
    virtual void compileShader(const MeshFeatureSet& meshFeatures,
      const MaterialFeatureSet& materialFeatures) = 0;

    // Textures
    //
    virtual RenderItemId addTexture(TexturePtr texture) = 0;
    virtual RenderItemId addNormalMap(TexturePtr texture) = 0;
    virtual RenderItemId addCubeMap(std::array<TexturePtr, 6>&& textures) = 0;

    virtual void removeTexture(RenderItemId id) = 0;
    virtual void removeCubeMap(RenderItemId id) = 0;

    // Meshes
    //
    virtual MeshHandle addMesh(MeshPtr mesh) = 0; // TODO: Remove
    virtual void removeMesh(RenderItemId id) = 0;
    virtual WorkItemResult addMeshAsync(MeshPtr mesh) = 0;
    virtual WorkItemResult removeMeshAsync(RenderItemId id) = 0;

    // Materials
    //
    virtual MaterialHandle addMaterial(MaterialPtr material) = 0;
    virtual void removeMaterial(RenderItemId id) = 0;

    // Per-frame draw functions
    //
    virtual void beginFrame(const Vec4f& clearColour) = 0;
    virtual void beginPass(RenderPass renderPass, const Vec3f& viewPos,
      const Mat4x4f& viewMatrix) = 0;
    virtual void setOrderKey(uint32_t order) = 0;
    virtual void setViewport(const Recti& viewport) = 0;
    virtual void drawModel(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform,
      const Vec4f& colour) = 0;
    virtual void drawModel(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform,
      const Vec4f& colour, const std::vector<Mat4x4f>& jointTransforms) = 0;
    virtual void drawInstance(MeshHandle mesh, MaterialHandle material,
      const Mat4x4f& transform) = 0;
    virtual void drawSprite(MeshHandle mesh, MaterialHandle material,
      const std::array<Vec2f, 4>& uvCoords, const Vec4f& colour, const Mat4x4f& transform) = 0;
    virtual void drawQuad(MeshHandle mesh, const Vec4f& colour, const Mat4x4f& transform) = 0;
    virtual void drawLight(const Vec3f& colour, float ambient, float specular, float zFar,
      const Mat4x4f& transform) = 0;  // TODO: Replace matrix with screen-space coords?
    virtual void drawDynamicText(MeshHandle mesh, MaterialHandle material, const std::string& text,
      const Vec4f& colour, const Mat4x4f& transform) = 0;
    virtual void drawSkybox(MeshHandle mesh, MaterialHandle cubeMap) = 0;
    virtual void endPass() = 0;
    virtual void endFrame() = 0;

    virtual ~Renderer() {}
};

using RendererPtr = std::unique_ptr<Renderer>;

} // namespace render

class FileSystem;
class WindowDelegate;
class Logger;

render::RendererPtr createRenderer(const FileSystem& fileSystem, WindowDelegate& window,
  Logger& logger);
