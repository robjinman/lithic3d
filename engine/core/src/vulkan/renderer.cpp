#include "vulkan/vulkan_utils.hpp"
#include "vulkan/pipeline.hpp"
#include "vulkan/render_resources.hpp"
#include "vulkan/vulkan_window_delegate.hpp"
#include "renderer.hpp"
#include "exception.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "utils.hpp"
#include "time.hpp"
#include "thread.hpp"
#include "triple_buffer.hpp"
#include "trace.hpp"
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>
#include <optional>
#include <fstream>
#include <atomic>
#include <set>
#include <map>
#include <cassert>
#include <iostream>

namespace render
{
namespace
{

const std::vector<const char*> ValidationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> DeviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#ifdef __APPLE__
  "VK_KHR_portability_subset",
#endif
};

using PipelineKey = ShaderProgramSpec;

struct QueueFamilyIndices
{
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() const
  {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails
{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

const HashedString strAddMesh = hashString("add_mesh");
const HashedString strRemoveMesh = hashString("remove_mesh");

struct AddMeshWorkItem : public WorkItem
{
  AddMeshWorkItem(MeshPtr mesh)
    : WorkItem(strAddMesh)
    , mesh(std::move(mesh)) {}

  MeshPtr mesh;
};

struct RemoveMeshWorkItem : public WorkItem
{
  RemoveMeshWorkItem(RenderItemId meshId)
    : WorkItem(strRemoveMesh)
    , meshId(meshId) {}

  RenderItemId meshId;
};

// 90 degrees clockwise
// TODO: Support other rotations
VkRect2D rotateRect(const VkRect2D& rect)
{
  return VkRect2D{
    .offset {
      .x = rect.offset.y,
      .y = rect.offset.x
    },
    .extent{
      .width = rect.extent.height,
      .height = rect.extent.width
    }
  };
}

// 90 degrees clockwise
// TODO: Support other rotations
ScreenMargins rotateMargins(const ScreenMargins& margins)
{
  return ScreenMargins{
    .left = margins.bottom,
    .right = margins.top,
    .top = margins.left,
    .bottom = margins.right
  };
}

class RendererImpl : public Renderer
{
  public:
    RendererImpl(FileSystem& fileSystem, VulkanWindowDelegate& window, Logger& logger,
      const ScreenMargins& margins);

    void start() override;
    bool isStarted() const override;
    void onResize() override;
    double frameRate() const override;
    // TODO: Is this just the perspective projection? What about orthographic?
    const ViewParams& getViewParams() const override;
    Vec2i getScreenSize() const override;
    Vec2i getViewportSize() const override;
    const ScreenMargins& getMargins() const override;
    void checkError() const override;

    // Initialisation
    //
    void compileShader(const MeshFeatureSet& meshFeatures,
      const MaterialFeatureSet& materialFeatures) override;

    // Resources
    //
    RenderItemId addTexture(TexturePtr texture) override;
    RenderItemId addNormalMap(TexturePtr texture) override;
    RenderItemId addCubeMap(std::array<TexturePtr, 6>&& textures) override;
    void removeTexture(RenderItemId id) override;
    void removeCubeMap(RenderItemId id) override;

    // Meshes
    //
    MeshHandle addMesh(MeshPtr mesh) override;
    void removeMesh(RenderItemId id) override;
    WorkItemResult addMeshAsync(MeshPtr mesh) override;
    WorkItemResult removeMeshAsync(RenderItemId id) override;

    // Materials
    //
    MaterialHandle addMaterial(MaterialPtr material) override;
    void removeMaterial(RenderItemId id) override;

    // Per frame draw functions
    //
    void beginFrame(const Vec4f& clearColour) override;
    void beginPass(RenderPass renderPass, const Vec3f& viewPos, const Mat4x4f& viewMatrix) override;
    void setOrderKey(uint32_t order) override;
    void setScissor(const Recti& scissor) override;
    void drawInstance(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform) override;
    void drawModel(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform,
      const Vec4f& colour) override;
    void drawModel(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform,
      const Vec4f& colour, const std::vector<Mat4x4f>& jointTransforms) override;
    void drawSprite(MeshHandle mesh, MaterialHandle material, const std::array<Vec2f, 4>& uvCoords,
      const Vec4f& colour, const Mat4x4f& transform) override;
    void drawQuad(MeshHandle mesh, const Vec4f& colour, const Mat4x4f& transform) override;
    void drawLight(const Vec3f& colour, float ambient, float specular, float zFar,
      const Mat4x4f& transform) override;
    void drawDynamicText(MeshHandle mesh, MaterialHandle material, const std::string& text,
      const Vec4f& colour, const Mat4x4f& transform) override;
    void drawSkybox(MeshHandle mesh, MaterialHandle material) override;
    void endPass() override;
    void endFrame() override;

    ~RendererImpl() override;

  private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
      const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData);

    void createInstance();
    void pickPhysicalDevice();
    bool isPhysicalDeviceSuitable(VkPhysicalDevice device) const;
    void checkValidationLayerSupport() const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    std::vector<const char*> getRequiredExtensions() const;
    void setupDebugMessenger();
    void destroyDebugMessenger();
    VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo() const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    void createLogicalDevice();
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSwapChainSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR chooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    void createSwapChain();
    void createSwapChain(VkExtent2D extent);
    void recreateSwapChain();
    void setOrthographicMatrix(float rotation);
    void setPerspectiveMatrix(float rotation);
    void cleanupSwapChain();
    void createImageViews();
    void createCommandPool();
    void createDepthResources();
    void createCommandBuffers();
    void doShadowRenderPass(VkCommandBuffer commandBuffer);
    void doMainRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void doOverlayRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void doSsrRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void updateLightingUbo(RenderPass renderPass);
    void updateLightTransformsUbo();
    void updateCameraTransformsUbo(RenderPass renderPass);
    void finishFrame();
    void createSyncObjects();
    void renderLoop();
    void cleanUp();
    void recordCommandBuffer(RenderPass renderPass, const RenderGraph& renderGraph,
      const std::vector<VkRect2D>& scissors, VkCommandBuffer commandBuffer);
    RenderGraph::Key generateRenderGraphKey(uint32_t orderKey, MeshHandle mesh,
      MaterialHandle material) const;
    Pipeline& choosePipeline(RenderPass renderPass, const RenderNode& node);
    void drawModelInternal(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform,
      const Vec4f& colour, const std::optional<std::vector<Mat4x4f>>& jointTransforms);
    void processWorkItem(WorkItem& item, std::promise<WorkItemResultValuePtr>& result);

    ScreenMargins m_margins;
    ShaderSystemPtr m_shaderSystem;
    ViewParams m_viewParams;
    VulkanWindowDelegate& m_window;
    Logger& m_logger;
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceLimits m_deviceLimits;
    VkSurfaceKHR m_surface;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainImageFormat;
    VkExtent2D m_swapchainExtent;
    bool m_viewRotated = false;
    // Equals swapchain extent (with width/height swapped if rotated) with margins subtracted
    Vec2i m_viewDimensions;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkImage> m_swapchainImages;
    VkImage m_depthImage;
    VkDeviceMemory m_depthImageMemory;
    VkImageView m_depthImageView;
    std::vector<VkCommandBuffer> m_commandBuffers;
    uint32_t m_imageIndex;
    VkCommandPool m_commandPool;

    size_t m_currentFrame = 0;
    std::atomic<bool> m_framebufferResized = false;
    Mat4x4f m_perspectiveMatrix;
    Mat4x4f m_orthographicMatrix;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    struct RenderPassState
    {
      RenderGraph graph;
      std::map<RenderGraph::Key, RenderNode*> lookup;
      Vec3f viewPos;
      Mat4x4f viewMatrix;
    };

    struct LightState
    {
      Vec3f position;
      Vec3f direction;
      Vec3f colour;
      float ambient;
      float specular;
      float zFar;
    };

    struct LightingState
    {
      uint32_t numLights;
      LightState lights[MAX_LIGHTS];
    };

    struct FrameState
    {
      std::map<RenderPass, RenderPassState> renderPasses;
      LightingState lighting;
      Vec4f clearColour;
      std::optional<RenderPass> currentRenderPass;
      uint32_t currentOrderKey = 0;
      std::vector<VkRect2D> scissors;
      uint32_t currentScissor = 0;
    };

    TripleBuffer<FrameState> m_frameStates;
  
    RenderResourcesPtr m_resources;
    std::unordered_map<PipelineKey, PipelinePtr> m_pipelines;

    Timer m_timer;
    std::atomic<double> m_frameRate;

    Thread m_thread;
    std::atomic<bool> m_running;
    mutable std::mutex m_errorMutex;
    bool m_hasError = false;
    std::string m_error;
    WorkQueue m_workQueue;
};

RendererImpl::RendererImpl(FileSystem& fileSystem, VulkanWindowDelegate& window, Logger& logger,
  const ScreenMargins& margins)
  : m_margins(margins)
  , m_window(window)
  , m_logger(logger)
{
  DBG_TRACE(m_logger);

  m_shaderSystem = createShaderSystem(fileSystem, m_logger);

  m_viewParams = ViewParams{
    .hFov = 0.f,
    .vFov = degreesToRadians(45.f),
    .aspectRatio = 0,
    .nearPlane = 0.1f,
    .farPlane = 10000.f
  };

  m_frameStates.getReadable().renderPasses[RenderPass::Main] = RenderPassState{};

  m_thread.run<void>([this]() {
    createInstance();
#ifndef NDEBUG
    setupDebugMessenger();
#endif
  }).get();
  m_surface = m_window.createSurface(m_instance);
  m_thread.run<void>([this]() {
    pickPhysicalDevice();
    createLogicalDevice();
  }).get();
  createSwapChain();
  m_thread.run<void>([this]() {
    createImageViews();
    createCommandPool();
    m_resources = createRenderResources(m_physicalDevice, m_device, m_graphicsQueue, m_commandPool,
      m_logger);
    createDepthResources();
    createCommandBuffers();
    createSyncObjects();
  }).get();
}

void RendererImpl::start()
{
  m_running = true;
  m_thread.run<void>([&]() {
    renderLoop();
  });
}

bool RendererImpl::isStarted() const
{
  return m_running;
}

void RendererImpl::checkError() const
{
  std::lock_guard lock(m_errorMutex);

  if (m_hasError) {
    EXCEPTION(m_error);
  }
}

void RendererImpl::compileShader(const MeshFeatureSet& meshFeatures,
  const MaterialFeatureSet& materialFeatures)
{
  ASSERT(!m_running, "Renderer already started");

  return m_thread.run<void>([&, this]() {
    auto depthFormat = findDepthFormat(m_physicalDevice);

    auto addPipeline = [this, depthFormat](PipelineKey key, VkExtent2D extent) {
      auto shader = m_shaderSystem->compileShaderProgram(key);

      if (!m_pipelines.contains(key)) {
        auto pipeline = createPipeline(key, shader, *m_resources, m_logger, m_device, extent,
          m_swapchainImageFormat, depthFormat,
          m_viewRotated ? rotateMargins(m_margins) : m_margins);

        m_pipelines.insert(std::make_pair(key, std::move(pipeline)));
      }
    };

    // TODO: Bit wasteful creating identical pipelines for main and overlay
/*
    addPipeline(PipelineKey{
      .renderPass = RenderPass::Main,
      .meshFeatures = meshFeatures,
      .materialFeatures = materialFeatures
    }, m_swapchainExtent);*/

    addPipeline(PipelineKey{
      .renderPass = RenderPass::Overlay,
      .meshFeatures = meshFeatures,
      .materialFeatures = materialFeatures
    }, m_swapchainExtent);

    if (meshFeatures.flags.test(MeshFeatures::CastsShadow)) {
       addPipeline(PipelineKey{
        .renderPass = RenderPass::Shadow,
        .meshFeatures = meshFeatures,
        .materialFeatures = {}
      }, VkExtent2D{ SHADOW_MAP_W, SHADOW_MAP_H });
    }
  }).get();
}

double RendererImpl::frameRate() const
{
  return m_frameRate;
}

void RendererImpl::onResize()
{
  m_framebufferResized = true;
}

const ViewParams& RendererImpl::getViewParams() const
{
  return m_viewParams;
}

Vec2i RendererImpl::getScreenSize() const
{
  return {
    static_cast<int>(m_swapchainExtent.width),
    static_cast<int>(m_swapchainExtent.height)
  };
}

Vec2i RendererImpl::getViewportSize() const
{
  return m_viewDimensions;
}

const ScreenMargins& RendererImpl::getMargins() const
{
  return m_margins;
}

RenderGraph::Key RendererImpl::generateRenderGraphKey(uint32_t orderKey, MeshHandle mesh,
  MaterialHandle material) const
{
  PipelineKey pipelineKey{
    .renderPass = RenderPass::Main,
    .meshFeatures = mesh.features,
    .materialFeatures = material.features
  };
  auto pipelineHash = std::hash<PipelineKey>{}(pipelineKey);

  if (mesh.features.flags.test(MeshFeatures::IsInstanced)) {
    return RenderGraph::Key{
      static_cast<RenderGraphKey>(orderKey),
      static_cast<RenderGraphKey>(material.features.flags.test(MaterialFeatures::HasTransparency)),
      static_cast<RenderGraphKey>(pipelineHash),
      static_cast<RenderGraphKey>(mesh.id),
      static_cast<RenderGraphKey>(material.id)
    };
  }
  else if (mesh.features.flags.test(MeshFeatures::IsSkybox)) {
    return RenderGraph::Key{
      static_cast<RenderGraphKey>(orderKey),
      static_cast<RenderGraphKey>(material.features.flags.test(MaterialFeatures::HasTransparency)),
      static_cast<RenderGraphKey>(pipelineHash),
    };
  }
  else {
    static RenderGraphKey nextId = 0;

    return RenderGraph::Key{
      static_cast<RenderGraphKey>(orderKey),
      static_cast<RenderGraphKey>(material.features.flags.test(MaterialFeatures::HasTransparency)),
      static_cast<RenderGraphKey>(pipelineHash),
      static_cast<RenderGraphKey>(mesh.id),
      static_cast<RenderGraphKey>(material.id),
      nextId++
    };
  }
}

void RendererImpl::drawInstance(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform)
{
  //DBG_TRACE(m_logger);

  FrameState& frameState = m_frameStates.getWritable();
  RenderPassState& state = frameState.renderPasses.at(frameState.currentRenderPass.value());
  RenderGraph& renderGraph = state.graph;

  auto key = generateRenderGraphKey(frameState.currentOrderKey, mesh, material);
  InstancedModelNode* node = nullptr;
  auto i = state.lookup.find(key);
  if (i != state.lookup.end()) {
    node = dynamic_cast<InstancedModelNode*>(i->second);
  }
  else {
    auto newNode = std::make_unique<InstancedModelNode>();
    newNode->mesh = mesh;
    newNode->material = material;
    newNode->scissorId = frameState.currentScissor;
    node = newNode.get();
    renderGraph.insert(key, std::move(newNode));
    state.lookup.insert({ key, node });
  }
  node->instances.push_back(MeshInstance{transform * mesh.transform});
}

void RendererImpl::drawSprite(MeshHandle mesh, MaterialHandle material,
  const std::array<Vec2f, 4>& uvCoords, const Vec4f& colour, const Mat4x4f& transform)
{
  //DBG_TRACE(m_logger);

  FrameState& frameState = m_frameStates.getWritable();
  RenderPassState& state = frameState.renderPasses.at(frameState.currentRenderPass.value());
  RenderGraph& renderGraph = state.graph;

  auto node = std::make_unique<SpriteNode>();
  node->mesh = mesh;
  node->material = material;
  node->modelMatrix = transform;
  node->uvCoords = uvCoords;
  node->colour = colour;
  node->scissorId = frameState.currentScissor;

  auto key = generateRenderGraphKey(frameState.currentOrderKey, mesh, material);

  state.lookup.insert({ key, node.get() });
  renderGraph.insert(key, std::move(node));
}

void RendererImpl::drawQuad(MeshHandle mesh, const Vec4f& colour, const Mat4x4f& transform)
{
  //DBG_TRACE(m_logger);

  FrameState& frameState = m_frameStates.getWritable();
  RenderPassState& state = frameState.renderPasses.at(frameState.currentRenderPass.value());
  RenderGraph& renderGraph = state.graph;

  auto node = std::make_unique<QuadNode>();
  node->mesh = mesh;
  node->modelMatrix = transform;
  node->colour = colour;
  node->scissorId = frameState.currentScissor;

  auto key = generateRenderGraphKey(frameState.currentOrderKey, mesh, node->material);

  state.lookup.insert({ key, node.get() });
  renderGraph.insert(key, std::move(node));
}

void RendererImpl::drawModel(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform,
  const Vec4f& colour, const std::vector<Mat4x4f>& jointTransforms)
{
  drawModelInternal(mesh, material, transform, colour, jointTransforms);
}

void RendererImpl::drawModel(MeshHandle mesh, MaterialHandle material, const Mat4x4f& transform,
  const Vec4f& colour)
{
  //DBG_TRACE(m_logger);

  drawModelInternal(mesh, material, transform, colour, std::nullopt);
}

void RendererImpl::drawModelInternal(MeshHandle mesh, MaterialHandle material,
  const Mat4x4f& transform, const Vec4f& colour,
  const std::optional<std::vector<Mat4x4f>>& jointTransforms)
{
  //DBG_TRACE(m_logger);

  FrameState& frameState = m_frameStates.getWritable();
  RenderPassState& state = frameState.renderPasses.at(frameState.currentRenderPass.value());
  RenderGraph& renderGraph = state.graph;

  auto node = std::make_unique<DefaultModelNode>();
  node->mesh = mesh;
  node->material = material;
  node->modelMatrix = transform;
  node->colour = colour;
  node->jointTransforms = jointTransforms;
  node->scissorId = frameState.currentScissor;

  auto key = generateRenderGraphKey(frameState.currentOrderKey, mesh, material);

  state.lookup.insert({ key, node.get() });
  renderGraph.insert(key, std::move(node));
}

void RendererImpl::drawLight(const Vec3f& colour, float ambient, float specular,
  float zFar, const Mat4x4f& transform)
{
  FrameState& frameState = m_frameStates.getWritable();
  ASSERT(frameState.lighting.numLights < MAX_LIGHTS, "Exceeded max lights");

  LightState& light = frameState.lighting.lights[frameState.lighting.numLights++];
  light.colour = colour;
  light.ambient = ambient;
  light.specular = specular;
  light.position = getTranslation(transform);
  light.direction = getDirection(transform);
  light.zFar = zFar;
}

void RendererImpl::drawDynamicText(MeshHandle mesh, MaterialHandle material,
  const std::string& text, const Vec4f& colour, const Mat4x4f& transform)
{
  //DBG_TRACE(m_logger);

  FrameState& frameState = m_frameStates.getWritable();
  RenderPassState& state = frameState.renderPasses.at(frameState.currentRenderPass.value());
  RenderGraph& renderGraph = state.graph;

  auto node = std::make_unique<DynamicTextNode>();
  node->mesh = mesh;
  node->material = material;
  node->modelMatrix = transform;
  node->text = text;
  node->colour = colour;
  node->scissorId = frameState.currentScissor;

  auto key = generateRenderGraphKey(frameState.currentOrderKey, mesh, material);

  state.lookup.insert({ key, node.get() });
  renderGraph.insert(key, std::move(node));
}

void RendererImpl::drawSkybox(MeshHandle mesh, MaterialHandle material)
{
  //DBG_TRACE(m_logger);

  FrameState& frameState = m_frameStates.getWritable();
  RenderPassState& state = frameState.renderPasses.at(frameState.currentRenderPass.value());
  RenderGraph& renderGraph = state.graph;

  auto node = std::make_unique<SkyboxNode>();
  node->mesh = mesh;
  node->material = material;
  node->scissorId = frameState.currentScissor;

  auto key = generateRenderGraphKey(frameState.currentOrderKey, mesh, material);

  state.lookup.insert({ key, node.get() });
  renderGraph.insert(key, std::move(node));
}

void RendererImpl::beginFrame(const Vec4f& clearColour)
{
  DBG_TRACE(m_logger);

  auto& state = m_frameStates.getWritable();
  state.lighting = LightingState{};
  state.clearColour = clearColour;
  state.currentRenderPass = std::nullopt;
  state.renderPasses.clear();
  state.scissors.clear();
  state.scissors.push_back({ VkOffset2D{0, 0}, m_swapchainExtent });
  state.currentScissor = 0;
}

void RendererImpl::setOrderKey(uint32_t order)
{
  m_frameStates.getWritable().currentOrderKey = order;
}

void RendererImpl::setScissor(const Recti& scissor)
{
  auto& state = m_frameStates.getWritable();
  uint32_t index = static_cast<uint32_t>(state.scissors.size());
  state.scissors.push_back(VkRect2D{
    .offset{
      .x = scissor.x,
      .y = scissor.y
    },
    .extent{
      .width = static_cast<uint32_t>(scissor.w),
      .height = static_cast<uint32_t>(scissor.h)
    }
  });
  state.currentScissor = index;
}

void RendererImpl::beginPass(RenderPass renderPass, const Vec3f& viewPos, const Mat4x4f& viewMatrix)
{
  DBG_TRACE(m_logger);

  auto& state = m_frameStates.getWritable();
  state.currentRenderPass = renderPass;

  auto& renderPassState = state.renderPasses[renderPass];
  renderPassState.viewPos = viewPos;
  renderPassState.viewMatrix = viewMatrix;
}

void RendererImpl::processWorkItem(WorkItem& item, std::promise<WorkItemResultValuePtr>& result)
{
  try {
    if (item.name == strAddMesh) {
      auto& data = dynamic_cast<AddMeshWorkItem&>(item);
      result.set_value(std::make_unique<AddMeshResult>(m_resources->addMesh(std::move(data.mesh))));
    }
    else if (item.name == strRemoveMesh) {
      auto& data = dynamic_cast<const RemoveMeshWorkItem&>(item);
      m_resources->removeMesh(data.meshId);
      result.set_value(std::make_unique<RemoveMeshResult>());
    }
    else {
      throw std::runtime_error("Don't know how to handle work item");
    }
  }
  catch (const std::exception&) {
    result.set_exception(std::current_exception());
  }
}

void RendererImpl::renderLoop()
{
  try {
    auto handler = [this](WorkItem& item, std::promise<WorkItemResultValuePtr>& result) {
      processWorkItem(item, result);
    };

    while (m_running) {
      while (m_workQueue.processNext(handler)) {}

      VK_CHECK(vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX),
        "Error waiting for fence");

      VkResult acqImgResult = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

      if (acqImgResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_logger.info("acqImgResult == VK_ERROR_OUT_OF_DATE_KHR");
        recreateSwapChain();
        return;
      }
      else if (acqImgResult != VK_SUCCESS && acqImgResult != VK_SUBOPTIMAL_KHR) {
        EXCEPTION("Error obtaining image from swap chain");
      }

      VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]),
        "Error resetting fence");

      vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

      VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
      };

      auto commandBuffer = m_commandBuffers[m_currentFrame];

      VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "Failed to begin recording command buffer");

      auto& frameState = m_frameStates.getReadable();
      if (frameState.renderPasses.contains(RenderPass::Shadow)) {
        doShadowRenderPass(commandBuffer);
      }
      if (frameState.renderPasses.contains(RenderPass::Main)) {
        doMainRenderPass(commandBuffer, m_imageIndex);
      }
      if (frameState.renderPasses.contains(RenderPass::Ssr)) {
        doSsrRenderPass(commandBuffer, m_imageIndex);
      }
      if (frameState.renderPasses.contains(RenderPass::Overlay)) {
        doOverlayRenderPass(commandBuffer, m_imageIndex);
      }

      VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to record command buffer");

      finishFrame();

      m_frameStates.readComplete();

      m_frameRate = 1.0 / m_timer.elapsed();
      m_timer.reset();
    }
  }
  catch (const std::exception& e) {
    std::lock_guard lock(m_errorMutex);

    m_hasError = true;
    m_error = e.what();
    m_running = false;
  }

  cleanUp();
}

void RendererImpl::updateCameraTransformsUbo(RenderPass renderPass)
{
  auto& frameState = m_frameStates.getReadable();
  auto& renderPassState = frameState.renderPasses.at(renderPass);

  CameraTransformsUbo cameraTransformsUbo{
    .viewMatrix = renderPassState.viewMatrix,
    .projMatrix = renderPass == RenderPass::Overlay ? m_orthographicMatrix : m_perspectiveMatrix
  };

  m_resources->updateCameraTransformsUbo(cameraTransformsUbo, m_currentFrame);
}

void RendererImpl::updateLightTransformsUbo()
{
  auto& frameState = m_frameStates.getReadable();

  // TODO: Currently only the first light can cast shadows
  const LightState& light = frameState.lighting.lights[0];
  LightTransformsUbo lightTransformsUbo{
    .viewMatrix = lookAt(light.position, light.position + light.direction),
    .projMatrix = orthographic(PIf / 2.f, PIf / 2.f, 0.f, light.zFar)
  };
  m_resources->updateLightTransformsUbo(lightTransformsUbo, m_currentFrame);
}

void RendererImpl::updateLightingUbo(RenderPass renderPass)
{
  auto& frameState = m_frameStates.getReadable();
  auto& renderPassState = frameState.renderPasses.at(renderPass);

  LightingUbo lightingUbo{
    .viewPos = renderPassState.viewPos,
    .numLights = frameState.lighting.numLights,
    .lights{}
  };
  for (uint32_t i = 0; i < frameState.lighting.numLights; ++i) {
    auto& light = frameState.lighting.lights[i];

    lightingUbo.lights[i] = Light{
      .worldPos = light.position,
      .colour = light.colour,
      .ambient = light.ambient,
      .specular = light.specular
    };
  }

  m_resources->updateLightingUbo(lightingUbo, m_currentFrame);
}

void RendererImpl::endPass()
{
  m_frameStates.getWritable().currentRenderPass = std::nullopt;
}

void RendererImpl::endFrame()
{
  DBG_TRACE(m_logger);

  m_frameStates.writeComplete();
}

void RendererImpl::finishFrame()
{
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

  VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_imageIndex] };
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]),
    "Failed to submit draw command buffer");

  VkSwapchainKHR swapchains[] = { m_swapchain };

  VkPresentInfoKHR presentInfo{
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = nullptr,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signalSemaphores,
    .swapchainCount = 1,
    .pSwapchains = swapchains,
    .pImageIndices = &m_imageIndex,
    .pResults = nullptr
  };

  VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR
    || m_framebufferResized) {

    m_framebufferResized = false;
    recreateSwapChain();
  }
  else if (presentResult != VK_SUCCESS) {
    EXCEPTION("Failed to present swap chain image");
  }

  m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkExtent2D RendererImpl::chooseSwapChainExtent(
  const VkSurfaceCapabilitiesKHR& capabilities) const
{
  if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
    int width = 0;
    int height = 0;
    m_window.getFrameBufferSize(width, height);

    VkExtent2D extent = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height)
    };

    extent.width = std::max(capabilities.minImageExtent.width,
      std::min(capabilities.maxImageExtent.width, static_cast<uint32_t>(width)));
    extent.height = std::max(capabilities.minImageExtent.height,
      std::min(capabilities.maxImageExtent.height, static_cast<uint32_t>(height)));

    return extent;
  }
  else {
    return capabilities.currentExtent;
  }
}

VkPresentModeKHR RendererImpl::chooseSwapChainPresentMode(
  const std::vector<VkPresentModeKHR>& availableModes) const
{
  for (auto& mode : availableModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      //return mode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR RendererImpl::chooseSwapChainSurfaceFormat(
  const std::vector<VkSurfaceFormatKHR>& availableFormats) const
{
  m_logger.debug("Available surface formats:");
  for (const auto& format : availableFormats) {
    m_logger.debug(STR("Format = " << format.format << ", colourSpace = " << format.colorSpace));
  }

  for (const auto& format : availableFormats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
      format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

      return format;
    }
    if (format.format == VK_FORMAT_R8G8B8A8_UNORM &&
      format.colorSpace == VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT) {

      return format;
    }
  }

  assert(availableFormats.size() > 0);

  m_logger.warn("Preferred swap chain surface format not available");

  return availableFormats[0];
}

void RendererImpl::createSwapChain()
{
  DBG_TRACE(m_logger);

  auto swapchainSupport = querySwapChainSupport(m_physicalDevice);
  auto extent = chooseSwapChainExtent(swapchainSupport.capabilities);
  createSwapChain(extent);
}

void RendererImpl::createSwapChain(VkExtent2D extent)
{
  m_logger.info(STR("Creating swapchain with extent "
    << extent.width << ", " << extent.height));

  auto swapchainSupport = querySwapChainSupport(m_physicalDevice);
  auto surfaceFormat = chooseSwapChainSurfaceFormat(swapchainSupport.formats);
  auto presentMode = chooseSwapChainPresentMode(swapchainSupport.presentModes);

  uint32_t minImageCount = swapchainSupport.capabilities.minImageCount + 1;
  if (swapchainSupport.capabilities.maxImageCount != 0) {
    minImageCount = std::min(minImageCount, swapchainSupport.capabilities.maxImageCount);
  }

  m_viewDimensions = {
    static_cast<int>(extent.width - m_margins.left - m_margins.right),
    static_cast<int>(extent.height - m_margins.top - m_margins.bottom)
  };

  m_logger.info(STR("Surface capabilities currentTransform = "
    << swapchainSupport.capabilities.currentTransform));

  // TODO: Support 270 rotation
  float rotation = 0;
  if (swapchainSupport.capabilities.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
    rotation = degreesToRadians(90.f);
    std::swap(extent.width, extent.height);
    m_viewRotated = true;
  }
  else if (swapchainSupport.capabilities.currentTransform
    & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {

    EXCEPTION("Unsupported surface transform");
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_surface;
  createInfo.minImageCount = minImageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  auto indices = findQueueFamilies(m_physicalDevice);
  uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
  if (indices.graphicsFamily == indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  else {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  }

  createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = m_swapchain;

  VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain),
    "Failed to create swap chain");

  uint32_t imageCount;
  VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr),
    "Failed to retrieve swap chain images");

  m_swapchainImages.resize(imageCount);
  VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data()),
    "Failed to retrieve swap chain images");

  m_swapchainImageFormat = surfaceFormat.format;
  m_swapchainExtent = extent;

  setOrthographicMatrix(rotation);
  setPerspectiveMatrix(rotation);
}

void RendererImpl::setOrthographicMatrix(float rotation)
{
  float aspect = static_cast<float>(m_viewDimensions[0]) / m_viewDimensions[1];

  m_logger.info(STR("Creating orthographic matrix from viewport dimensions "
    << m_viewDimensions[0] << ", " << m_viewDimensions[1]));

  // TODO: Move to math.cpp

  Mat4x4f m;
  const float t = 0.5f;
  const float b = -0.5f;
  const float r = 0.5f * aspect;
  const float l = -0.5f * aspect;
  const float f = 100.f;
  const float n = 0.f;

  m.set(0, 0, 2.f / (r - l));
  m.set(0, 3, (r + l) / (l - r));
  m.set(1, 1, 2.f / (b - t));
  m.set(1, 3, (b + t) / (b - t));
  m.set(2, 2, 1.f / (f - n));
  m.set(2, 3, -n / (f - n));
  m.set(3, 3, 1.f);

  Mat4x4f rot = rotationMatrix4x4<float>(Vec3f{ 0.f, 0.f, rotation });
  m_orthographicMatrix = rot * m;
}

void RendererImpl::setPerspectiveMatrix(float rotation)
{
  float aspect = static_cast<float>(m_viewDimensions[0]) / m_viewDimensions[1];

  m_viewParams.aspectRatio = aspect;
  m_viewParams.hFov = 2.f * atan(aspect * tan(0.5f * m_viewParams.vFov));

  Mat4x4f rot = rotationMatrix4x4<float>(Vec3f{ 0.f, 0.f, rotation });
  m_perspectiveMatrix = rot * perspective(m_viewParams.hFov, m_viewParams.vFov,
    m_viewParams.nearPlane, m_viewParams.farPlane);
}

void RendererImpl::cleanupSwapChain()
{
  vkDestroyImageView(m_device, m_depthImageView, nullptr);
  vkDestroyImage(m_device, m_depthImage, nullptr);
  vkFreeMemory(m_device, m_depthImageMemory, nullptr);
  for (auto imageView : m_swapchainImageViews) {
    vkDestroyImageView(m_device, imageView, nullptr);
  }
  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

void RendererImpl::recreateSwapChain()
{
  int width = 0;
  int height = 0;
  m_window.getFrameBufferSize(width, height);

  VK_CHECK(vkDeviceWaitIdle(m_device), "Error waiting for device to be idle");

  VkExtent2D extent{
    .width = static_cast<uint32_t>(width),
    .height = static_cast<uint32_t>(height)
  };

  cleanupSwapChain();
  createSwapChain(extent);
  createImageViews();
  createDepthResources();

  for (auto& i : m_pipelines) {
    if (i.first.renderPass == RenderPass::Main || i.first.renderPass == RenderPass::Overlay) {
      i.second->onViewportResize(m_swapchainExtent);
    }
  }
}

SwapChainSupportDetails RendererImpl::querySwapChainSupport(VkPhysicalDevice device) const
{
  SwapChainSupportDetails details;

  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities),
    "Failed to retrieve physical device surface capabilities");

  uint32_t formatCount;
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr),
    "Failed to retrieve physical device surface formats");

  details.formats.resize(formatCount);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount,
    details.formats.data()), "Failed to retrieve physical device surface formats");

  uint32_t presentModeCount;
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr),
    "Failed to retrieve physical device surface present modes");

  details.presentModes.resize(presentModeCount);
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount,
    details.presentModes.data()), "Failed to retrieve physical device surface present modes");

  return details;
}

QueueFamilyIndices RendererImpl::findQueueFamilies(VkPhysicalDevice device) const
{
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport),
      "Failed to check present support for device");

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }
  }

  return indices;
}

VKAPI_ATTR VkBool32 VKAPI_CALL RendererImpl::debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
  const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
  std::cerr << "Validation layer: " << data->pMessage << std::endl;

  return VK_FALSE;
}

std::vector<const char*> RendererImpl::getRequiredExtensions() const
{
  std::vector<const char*> extensions{
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
  };

#ifdef __APPLE__
  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
  auto windowExtensions = m_window.getRequiredExtensions();
  extensions.insert(extensions.end(), windowExtensions.begin(), windowExtensions.end());

#ifndef NDEBUG
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  return extensions;
}

bool RendererImpl::checkDeviceExtensionSupport(VkPhysicalDevice device) const
{
  uint32_t count;
  VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr),
    "Failed to enumerate device extensions");

  std::vector<VkExtensionProperties> available(count);

  VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data()),
    "Failed to enumerate device extensions");

  for (auto extension : DeviceExtensions) {
    auto fnMatches = [=](const VkExtensionProperties& p) {
      return strcmp(extension, p.extensionName) == 0;
    };
    if (std::find_if(available.begin(), available.end(), fnMatches) == available.end()) {
      return false;
    }
  }
  return true;
}

void RendererImpl::checkValidationLayerSupport() const
{
  uint32_t layerCount;
  VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr),
    "Failed to enumerate instance layer properties");

  std::vector<VkLayerProperties> available(layerCount);
  VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, available.data()),
    "Failed to enumerate instance layer properties");

  for (auto layer : ValidationLayers) {
    auto fnMatches = [=](const VkLayerProperties& p) {
      return strcmp(layer, p.layerName) == 0;
    };
    if (std::find_if(available.begin(), available.end(), fnMatches) == available.end()) {
      EXCEPTION("Validation layer '" << layer << "' not supported");
    }
  }
}

bool RendererImpl::isPhysicalDeviceSuitable(VkPhysicalDevice device) const
{
  bool extensionsSupported = checkDeviceExtensionSupport(device);

  if (!extensionsSupported) {
    m_logger.warn("Extensions not supported");
    return false;
  }

  auto swapchainSupport = querySwapChainSupport(device);
  bool swapchainAdequate = !swapchainSupport.formats.empty() &&
                           !swapchainSupport.presentModes.empty();

  auto indices = findQueueFamilies(device);

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  return swapchainAdequate && indices.isComplete() && supportedFeatures.samplerAnisotropy;
}

void RendererImpl::createLogicalDevice()
{
  DBG_TRACE(m_logger);

  auto indices = findQueueFamilies(m_physicalDevice);
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {
    indices.graphicsFamily.value(),
    indices.presentFamily.value()
  };

  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};

    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    queueCreateInfos.push_back(queueCreateInfo);
  }

  /*
  VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
  indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
  indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;*/

  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
  dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
  dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

  VkPhysicalDeviceFeatures2 deviceFeatures2{};
  deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  deviceFeatures2.pNext = &dynamicRenderingFeatures;
  deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pNext = &deviceFeatures2;
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = nullptr;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
  createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

#ifdef NDEBUG
  createInfo.enabledLayerCount = 0;
#else
  createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
  createInfo.ppEnabledLayerNames = ValidationLayers.data();
#endif

  VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device),
    "Failed to create logical device");

  vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
  vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void RendererImpl::createImageViews()
{
  DBG_TRACE(m_logger);

  m_swapchainImageViews.resize(m_swapchainImages.size());

  for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
    m_swapchainImageViews[i] = createImageView(m_device, m_swapchainImages[i],
      m_swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
  }
}

void RendererImpl::createCommandPool()
{
  DBG_TRACE(m_logger);

  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
  VkCommandPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
  };

  VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool),
    "Failed to create command pool");
}

Pipeline& RendererImpl::choosePipeline(RenderPass renderPass, const RenderNode& node)
{
  PipelineKey key{
    .renderPass = renderPass,
    .meshFeatures = node.mesh.features,
    .materialFeatures = node.material.features
  };
  if (renderPass == RenderPass::Shadow) {
    key.materialFeatures = {};
  }
  auto i = m_pipelines.find(key);
  if (i == m_pipelines.end()) {
    EXCEPTION("No shader has been compiled for this combination of pass/mesh/material features");
  }
  return *i->second;
}

void RendererImpl::recordCommandBuffer(RenderPass renderPass, const RenderGraph& renderGraph,
  const std::vector<VkRect2D>& scissors, VkCommandBuffer commandBuffer)
{
  uint32_t scissorId = std::numeric_limits<uint32_t>::max();
  Pipeline* prevPipeline = nullptr;
  BindState bindState{};

  for (auto& node : renderGraph) {
    switch (node->type) {
      case RenderNodeType::DefaultModel: {
        auto& modelNode = dynamic_cast<const DefaultModelNode&>(*node);
        if (modelNode.jointTransforms.has_value()) {
          auto& joints = modelNode.jointTransforms.value();
          m_resources->updateJointTransforms(modelNode.mesh.id, joints, m_currentFrame);
        }
        break;
      }
      case RenderNodeType::InstancedModel: {
        auto& instancedNode = dynamic_cast<const InstancedModelNode&>(*node);
        m_resources->updateMeshInstances(instancedNode.mesh.id, instancedNode.instances);
        break;
      }
      default: break;
    }

    auto& pipeline = choosePipeline(renderPass, *node);

    if (node->scissorId != scissorId || &pipeline != prevPipeline) {
      scissorId = node->scissorId;
      auto margins = m_viewRotated ? rotateMargins(m_margins) : m_margins;
      auto rect = m_viewRotated ? rotateRect(scissors[scissorId]) : scissors[scissorId];
      rect.offset.x += margins.left;
      rect.offset.y += margins.top;

      //VkRect2D rect{
      //  .offset{},
      //  .extent = m_swapchainExtent
      //};

      vkCmdSetScissor(commandBuffer, 0, 1, &rect);
      prevPipeline = &pipeline;
    }

    pipeline.recordCommandBuffer(commandBuffer, *node, bindState, m_currentFrame);
  }
}

void RendererImpl::doShadowRenderPass(VkCommandBuffer commandBuffer)
{
  updateLightTransformsUbo();

  VkImageMemoryBarrier barrier1{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = m_resources->getShadowMapImage(),
    .subresourceRange = VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);

  VkRenderingAttachmentInfo depthAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = m_resources->getShadowMapImageView(),
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = nullptr,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = VkClearValue{
      .depthStencil = VkClearDepthStencilValue{
        .depth = 1.f,
        .stencil = 0
      }
    }
  };

  VkRenderingInfo renderingInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = VkRect2D{VkOffset2D{}, VkExtent2D{ SHADOW_MAP_W, SHADOW_MAP_H }},
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 0,
    .pColorAttachments = nullptr,
    .pDepthAttachment = &depthAttachment,
    .pStencilAttachment = nullptr
  };

  vkCmdBeginRenderingFn(commandBuffer, &renderingInfo);

  auto& frameState = m_frameStates.getReadable();
  auto& renderPassState = frameState.renderPasses.at(RenderPass::Shadow);
  const auto& renderGraph = renderPassState.graph;

  recordCommandBuffer(RenderPass::Shadow, renderGraph, frameState.scissors, commandBuffer);

  vkCmdEndRenderingFn(commandBuffer);

  VkImageMemoryBarrier barrier2{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = m_resources->getShadowMapImage(),
    .subresourceRange = VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);
}

void RendererImpl::doMainRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
  updateCameraTransformsUbo(RenderPass::Main);
  updateLightingUbo(RenderPass::Main);

  auto& frameState = m_frameStates.getReadable();
  auto& colour = frameState.clearColour;

  VkImageMemoryBarrier barrier1{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = m_swapchainImages[imageIndex],
    .subresourceRange = VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);

  VkRenderingAttachmentInfo colourAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = m_swapchainImageViews[imageIndex],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = nullptr,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = VkClearValue{
      .color = VkClearColorValue{ .float32 = { colour[0], colour[1], colour[2], colour[3] }}
    }
  };

  VkRenderingAttachmentInfo depthAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = m_depthImageView,
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = nullptr,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = VkClearValue{
      .depthStencil = VkClearDepthStencilValue{
        .depth = 1.f,
        .stencil = 0
      }
    }
  };

  VkRenderingInfo renderingInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = VkRect2D{VkOffset2D{}, m_swapchainExtent},
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colourAttachment,
    .pDepthAttachment = &depthAttachment,
    .pStencilAttachment = nullptr
  };

  vkCmdBeginRenderingFn(commandBuffer, &renderingInfo);

  auto& renderPassState = frameState.renderPasses.at(RenderPass::Main);
  const auto& renderGraph = renderPassState.graph;

  recordCommandBuffer(RenderPass::Main, renderGraph, frameState.scissors, commandBuffer);

  vkCmdEndRenderingFn(commandBuffer);

  VkImageMemoryBarrier barrier2{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = m_swapchainImages[imageIndex],
    .subresourceRange = VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);
}

void RendererImpl::doOverlayRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
  updateCameraTransformsUbo(RenderPass::Overlay);

  auto& frameState = m_frameStates.getReadable();
  auto& colour = frameState.clearColour;

  VkImageMemoryBarrier barrier1{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = m_swapchainImages[imageIndex],
    .subresourceRange = VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);

  VkRenderingAttachmentInfo colourAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = nullptr,
    .imageView = m_swapchainImageViews[imageIndex],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = nullptr,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = VkClearValue{
      .color = VkClearColorValue{ .float32 = { colour[0], colour[1], colour[2], colour[3] }}
    }
  };

  VkRenderingInfo renderingInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = VkRect2D{VkOffset2D{}, m_swapchainExtent},
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colourAttachment,
    .pDepthAttachment = nullptr,
    .pStencilAttachment = nullptr
  };

  vkCmdBeginRenderingFn(commandBuffer, &renderingInfo);

  auto& renderPassState = frameState.renderPasses.at(RenderPass::Overlay);
  const auto& renderGraph = renderPassState.graph;

  recordCommandBuffer(RenderPass::Overlay, renderGraph, frameState.scissors, commandBuffer);

  vkCmdEndRenderingFn(commandBuffer);

  VkImageMemoryBarrier barrier2{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = m_swapchainImages[imageIndex],
    .subresourceRange = VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);
}

void RendererImpl::doSsrRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
  // TODO
}

void RendererImpl::createCommandBuffers()
{
  DBG_TRACE(m_logger);

  m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = m_commandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size())
  };

  VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()),
    "Failed to allocate command buffers");
}

RenderItemId RendererImpl::addTexture(TexturePtr texture)
{
  DBG_TRACE(m_logger);

  ASSERT(!m_running, "Renderer already started");
  return m_thread.run<RenderItemId>([&, this]() {
    return m_resources->addTexture(std::move(texture));
  }).get();
}

RenderItemId RendererImpl::addNormalMap(TexturePtr texture)
{
  DBG_TRACE(m_logger);

  ASSERT(!m_running, "Renderer already started");
  return m_thread.run<RenderItemId>([&, this]() {
    return m_resources->addNormalMap(std::move(texture));
  }).get();
}

RenderItemId RendererImpl::addCubeMap(std::array<TexturePtr, 6>&& textures)
{
  DBG_TRACE(m_logger);

  ASSERT(!m_running, "Renderer already started");
  return m_thread.run<RenderItemId>([&, this]() {
    return m_resources->addCubeMap(std::move(textures));
  }).get();
}

MaterialHandle RendererImpl::addMaterial(MaterialPtr material)
{
  DBG_TRACE(m_logger);

  ASSERT(!m_running, "Renderer already started");
  return m_thread.run<MaterialHandle>([&, this]() {
    return m_resources->addMaterial(std::move(material));
  }).get();
}

MeshHandle RendererImpl::addMesh(MeshPtr mesh)
{
  DBG_TRACE(m_logger);

  ASSERT(!m_running, "Renderer is already running");

  return m_thread.run<MeshHandle>([&, this]() {
    return m_resources->addMesh(std::move(mesh));
  }).get();
}

void RendererImpl::removeMesh(RenderItemId meshId)
{
  DBG_TRACE(m_logger);

  ASSERT(!m_running, "Renderer is already running");

  return m_thread.run<void>([&, this]() {
    return m_resources->removeMesh(meshId);
  }).get();
}

WorkItemResult RendererImpl::addMeshAsync(MeshPtr mesh)
{
  DBG_TRACE(m_logger);

  auto item = std::make_unique<AddMeshWorkItem>(std::move(mesh));

  return m_workQueue.addWorkItem(std::move(item));
}

WorkItemResult RendererImpl::removeMeshAsync(RenderItemId meshId)
{
  DBG_TRACE(m_logger);

  auto item = std::make_unique<RemoveMeshWorkItem>(meshId);

  return m_workQueue.addWorkItem(std::move(item));
}

void RendererImpl::removeTexture(RenderItemId)
{
  // TODO
  EXCEPTION("Not implemented");
  //m_resources->removeTexture(id);
}

void RendererImpl::removeCubeMap(RenderItemId)
{
  // TODO
  EXCEPTION("Not implemented");
  //m_resources->removeCubeMap(id);
}

void RendererImpl::removeMaterial(RenderItemId)
{
  // TODO
  EXCEPTION("Not implemented");
  //m_resources->removeMaterial(id);
}

void RendererImpl::createSyncObjects()
{
  DBG_TRACE(m_logger);

  m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_renderFinishedSemaphores.resize(m_swapchainImages.size());
  m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]),
      "Failed to create semaphore");
    VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]),
      "Failed to create fence");
  }
  for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
    VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]),
      "Failed to create semaphore");
  }
}

void RendererImpl::createDepthResources()
{
  DBG_TRACE(m_logger);

  VkFormat depthFormat = findDepthFormat(m_physicalDevice);

  createImage(m_physicalDevice, m_device, m_swapchainExtent.width, m_swapchainExtent.height,
    depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);

  m_depthImageView = createImageView(m_device, m_depthImage, depthFormat,
    VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
}

void RendererImpl::pickPhysicalDevice()
{
  uint32_t deviceCount = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr),
    "Failed to enumerate physical devices");

  if (deviceCount == 0) {
    EXCEPTION("No physical devices found");
  }

  DBG_LOG(m_logger, STR("Found " << deviceCount << " devices"));

  std::vector<VkPhysicalDevice> devices(deviceCount);
  VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()),
    "Failed to enumerate physical devices");

  VkPhysicalDeviceProperties props;

  const std::map<int, size_t> deviceTypePriority{
    { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0 },
    { VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 1 },
    { VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU, 2 },
    { VK_PHYSICAL_DEVICE_TYPE_CPU, 3 },
    { VK_PHYSICAL_DEVICE_TYPE_OTHER, 4}
  };

  // (priority, device index)
  std::set<std::pair<size_t, size_t>> sortedDevices;

  for (size_t i = 0; i < deviceCount; ++i) {
    vkGetPhysicalDeviceProperties(devices[i], &props);

    DBG_LOG(m_logger, STR("Device: " << props.deviceName));
    DBG_LOG(m_logger, STR("Type: " << props.deviceType));

    DBG_LOG(m_logger, "Physical device properties");
    DBG_LOG(m_logger, STR("  maxComputeWorkGroupSize: "
      << props.limits.maxComputeWorkGroupSize[0] << ", "
      << props.limits.maxComputeWorkGroupSize[1] << ", "
      << props.limits.maxComputeWorkGroupSize[2]));
    DBG_LOG(m_logger, STR("  maxComputeWorkGroupCount: "
      << props.limits.maxComputeWorkGroupCount[0] << ", "
      << props.limits.maxComputeWorkGroupCount[1] << ", "
      << props.limits.maxComputeWorkGroupCount[2]));
    DBG_LOG(m_logger, STR("  maxComputeWorkGroupInvocations: "
      << props.limits.maxComputeWorkGroupInvocations));

    size_t priority = deviceTypePriority.at(props.deviceType);
    sortedDevices.insert(std::make_pair(priority, i));
  }

  assert(!sortedDevices.empty());
  int index = -1;
  for (auto& dev : sortedDevices) {
    if (isPhysicalDeviceSuitable(devices[dev.second])) {
      index = static_cast<int>(dev.second);
      break;
    }
  }
  if (index == -1) {
    EXCEPTION("No suitable physical device found");
  }

  vkGetPhysicalDeviceProperties(devices[index], &props);

  DBG_LOG(m_logger, STR("Selecting " << props.deviceName));
  m_deviceLimits = props.limits;

  m_physicalDevice = devices[index];
}

void RendererImpl::createInstance()
{
  DBG_TRACE(m_logger);

#ifndef NDEBUG
  checkValidationLayerSupport();
#endif

  VkApplicationInfo appInfo{
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = nullptr,
    .pApplicationName = "Minefield",
    .applicationVersion = VK_MAKE_VERSION(getVersionMajor(), getVersionMinor(), 0),
    .pEngineName = "No Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_2
  };

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
#ifdef __APPLE__
  createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
#ifdef NDEBUG
  createInfo.enabledLayerCount = 0;
  createInfo.pNext = nullptr;
#else
  createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
  createInfo.ppEnabledLayerNames = ValidationLayers.data();

  auto debugMessengerInfo = getDebugMessengerCreateInfo();
  createInfo.pNext = &debugMessengerInfo;
#endif

  auto extensions = getRequiredExtensions();

  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create instance");

  loadVulkanExtensionFunctions(m_instance);
}

VkDebugUtilsMessengerCreateInfoEXT RendererImpl::getDebugMessengerCreateInfo() const
{
  return VkDebugUtilsMessengerCreateInfoEXT{
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .pNext = nullptr,
    .flags = 0,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = debugCallback,
    .pUserData = nullptr
  };
}

void RendererImpl::setupDebugMessenger()
{
  auto createInfo = getDebugMessengerCreateInfo();

  auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
  if (func == nullptr) {
    EXCEPTION("Error getting pointer to vkCreateDebugUtilsMessengerEXT()");
  }
  VK_CHECK(func(m_instance, &createInfo, nullptr, &m_debugMessenger),
    "Error setting up debug messenger");
}

void RendererImpl::destroyDebugMessenger()
{
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
  func(m_instance, m_debugMessenger, nullptr);
}

void RendererImpl::cleanUp()
{
  vkDeviceWaitIdle(m_device);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
    vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
  }
  for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
    vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
  }
  vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  m_pipelines.clear();
  cleanupSwapChain();
  m_resources.reset();
#ifndef NDEBUG
  destroyDebugMessenger();
#endif
  vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
  vkDestroyDevice(m_device, nullptr);
  vkDestroyInstance(m_instance, nullptr);
}

RendererImpl::~RendererImpl()
{
  m_running = false;
}

} // namespace
} // namespace render

render::RendererPtr createRenderer(FileSystem& fileSystem, WindowDelegate& window, Logger& logger,
  const render::ScreenMargins& margins)
{
  return std::make_unique<render::RendererImpl>(fileSystem,
    dynamic_cast<VulkanWindowDelegate&>(window), logger, margins);
}
