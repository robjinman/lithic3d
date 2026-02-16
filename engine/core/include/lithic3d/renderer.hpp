#pragma once

#include "renderables.hpp"
#include "window_delegate.hpp"

namespace lithic3d
{
namespace render
{

enum class RenderPass
{
  Shadow0 = 0,
  Shadow1,
  Shadow2,
  Main,
  Ssr,
  Overlay
};

constexpr RenderPass shadowPass(uint32_t cascade)
{
  return static_cast<RenderPass>(static_cast<uint32_t>(RenderPass::Shadow0) + cascade);
}

inline bool isShadowPass(RenderPass renderPass)
{
  return renderPass >= RenderPass::Shadow0 && renderPass <= RenderPass::Shadow2;
}

struct ShaderProgramSpec
{
  RenderPass renderPass;
  MeshFeatureSet meshFeatures;
  MaterialFeatureSet materialFeatures;

  inline bool operator==(const ShaderProgramSpec& rhs) const
  {
    bool rp = renderPass == rhs.renderPass ||
      (isShadowPass(renderPass) && isShadowPass(rhs.renderPass));

    return meshFeatures == rhs.meshFeatures && materialFeatures == rhs.materialFeatures && rp;
  }

  std::string toString() const;
};

struct ScreenMargins
{
  uint32_t left = 0;
  uint32_t right = 0;
  uint32_t top = 0;
  uint32_t bottom = 0;
};

class Renderer
{
  public:
    virtual void start() = 0;
    virtual bool isStarted() const = 0;
    virtual double frameRate() const = 0;
    virtual void onResize() = 0;
    virtual Vec2i getScreenSize() const = 0;
    // Screen size after subtracting margins
    virtual Vec2i getViewportSize() const = 0;
    // Viewport rotation in radians. On some mobile devices, the viewport is rotated 90 or 270
    // degrees. This rotation needs to be baked into the projection matrix.
    virtual float getViewportRotation() const = 0;
    virtual const ScreenMargins& getMargins() const = 0;
    virtual void checkError() const = 0;

    // Initialisation
    //
    virtual void compileShader(const ShaderProgramSpec& spec) = 0;
    virtual bool hasCompiledShader(const ShaderProgramSpec& spec) const = 0;

    // Textures
    //
    virtual void addTexture(ResourceId id, TexturePtr texture) = 0;
    virtual void addNormalMap(ResourceId id, TexturePtr texture) = 0;
    virtual void addCubeMap(ResourceId id, std::array<TexturePtr, 6>&& textures) = 0;

    virtual void removeTexture(ResourceId id) = 0;
    virtual void removeCubeMap(ResourceId id) = 0;

    // Meshes
    //
    virtual void addMesh(ResourceId id, MeshPtr mesh) = 0;
    virtual void removeMesh(ResourceId id) = 0;

    // Materials
    //
    virtual void addMaterial(ResourceId id, MaterialPtr material) = 0;
    virtual void removeMaterial(ResourceId id) = 0;

    // Fonts
    //
    //virtual ResourceId addFont(const BitmapFont& font) = 0;

    // Per-frame draw functions
    //
    virtual void beginFrame(const Vec4f& clearColour) = 0;
    // TODO: Get viewPos from viewMatrix?
    virtual void beginPass(RenderPass renderPass, const Vec3f& viewPos,
      const Mat4x4f& viewMatrix, const Mat4x4f& projectionMatrix) = 0;
    virtual void setOrderKey(uint32_t order) = 0;
    virtual void setScissor(const Recti& scissor) = 0;
    virtual void drawModel(ResourceId mesh, const MeshFeatureSet& meshFeatures, ResourceId material,
      const MaterialFeatureSet& materialFeatures, const Vec4f& colour,
      const Mat4x4f& transform) = 0;
    virtual void drawModel(ResourceId mesh, const MeshFeatureSet& meshFeatures, ResourceId material,
      const MaterialFeatureSet& materialFeatures, const Vec4f& colour, const Mat4x4f& transform,
      const std::vector<Mat4x4f>& jointTransforms) = 0;
    virtual void drawInstance(ResourceId mesh, const MeshFeatureSet& meshFeatures,
      ResourceId material, const MaterialFeatureSet& materialFeatures,
      const Mat4x4f& transform) = 0;
    virtual void drawSprite(ResourceId mesh, const MeshFeatureSet& meshFeatures,
      ResourceId material, const MaterialFeatureSet& materialFeatures,
      const std::array<Vec2f, 4>& uvCoords, const Vec4f& colour, const Mat4x4f& transform) = 0; // TODO: Replace matrix with screen-space coords?
    virtual void drawQuad(ResourceId mesh, const MeshFeatureSet& meshFeatures, float radius,
      const Vec4f& colour, const Mat4x4f& transform) = 0; // TODO: Replace matrix with screen-space coords?
    virtual void drawPointLight(const Vec3f& colour, float ambient, float specular,
      const Mat4x4f& transform) = 0;
    virtual void drawDirectionalLight(const Vec3f& colour, float ambient, float specular,
      const Mat4x4f& transform) = 0;
    virtual void drawDynamicText(ResourceId mesh, const MeshFeatureSet& meshFeatures,
      ResourceId material, const MaterialFeatureSet& materialFeatures, const std::string& text,
      const Vec4f& colour, const Mat4x4f& transform) = 0; // TODO: Replace matrix with screen-space coords?
    virtual void drawSkybox(ResourceId mesh, const MeshFeatureSet& meshFeatures,
      ResourceId cubeMap, const MaterialFeatureSet& cubeMapFeatures) = 0;
    virtual void endPass() = 0;
    virtual void endFrame() = 0;

    virtual ~Renderer() {}
};

using RendererPtr = std::unique_ptr<Renderer>;

} // namespace render

class FileSystem;
class Logger;
class ResourceManager;

// TODO: Move inside render namespace?
render::RendererPtr createRenderer(WindowDelegatePtr window, ResourceManager& resourceManager,
  const FileSystem& fileSystem, Logger& logger, const render::ScreenMargins& margins);

} // namespace lithic3d

template<>
struct std::hash<lithic3d::render::ShaderProgramSpec>
{
  std::size_t operator()(const lithic3d::render::ShaderProgramSpec& spec) const noexcept
  {
    lithic3d::render::RenderPass renderPass = isShadowPass(spec.renderPass) ?
      lithic3d::render::RenderPass::Shadow0 : spec.renderPass;
    return lithic3d::hashAll(renderPass, spec.meshFeatures, spec.materialFeatures);
  }
};
