#include "lithic3d/terrain_builder.hpp"
#include "lithic3d/renderables.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/units.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/ecs.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_collision.hpp"
#include "lithic3d/model_loader.hpp"
#include <cassert>
#include <map>

namespace fs = std::filesystem;

namespace lithic3d
{

using render::Mesh;
using render::MeshPtr;
using render::MeshHandle;
using render::Material;
using render::MaterialHandle;
using render::Texture;
using render::BufferUsage;

namespace
{

struct TerrainRegion
{
  HeightMap heightMap;
  ResourceHandle model;
  Vec3f position;
};

std::string cellName(uint32_t x, uint32_t y)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << x << std::setw(3) << std::setfill('0') << y;
  return ss.str();
}

std::vector<float> constructHeightMap(const Mesh& mesh)
{
  size_t n = mesh.attributeBuffers[0].numElements();
  auto positions = reinterpret_cast<const Vec3f*>(mesh.attributeBuffers[0].data.data());

  std::vector<float> heightMap(n);

  for (size_t i = 0; i < n; ++i) {
    heightMap[i] = positions[i][1];
  }

  return heightMap;
}

class TerrainBuilderImpl : public TerrainBuilder
{
  public:
    TerrainBuilderImpl(const TerrainConfig& config, Ecs& ecs, ModelLoader& modelLoader,
      RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
      FileSystem& fileSystem, Logger& logger);

    ResourceHandle loadTerrainRegionAsync(uint32_t x, uint32_t y, XmlNodePtr terrainXml) override;
    std::vector<EntityId> createEntities(ResourceId regionId) override;

  private:
    TerrainConfig m_config;
    Logger& m_logger;
    Ecs& m_ecs;
    ModelLoader& m_modelLoader;
    RenderResourceLoader& m_renderResourceLoader;
    ResourceManager& m_resourceManager;
    FileSystem& m_fileSystem;
    std::map<ResourceId, TerrainRegion> m_regions;

    MeshPtr constructMesh(const Texture& heightMap) const;
};

TerrainBuilderImpl::TerrainBuilderImpl(const TerrainConfig& config, Ecs& ecs,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager,
  FileSystem& fileSystem, Logger& logger)
  : m_config(config)
  , m_logger(logger)
  , m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderResourceLoader(renderResourceLoader)
  , m_resourceManager(resourceManager)
  , m_fileSystem(fileSystem)
{
}

std::vector<EntityId> TerrainBuilderImpl::createEntities(ResourceId regionId)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();
  auto& sysCollision = m_ecs.system<SysCollision>();

  auto& region = m_regions.at(regionId);

  auto id = m_ecs.idGen().getNewEntityId();

  m_ecs.componentStore().allocate<DSpatial, DModel>(id);

  float minHeight = metresToWorldUnits(m_config.minHeight);
  float maxHeight = metresToWorldUnits(m_config.maxHeight);
  float cellWidth = metresToWorldUnits(m_config.cellWidth);
  float cellHeight = metresToWorldUnits(m_config.cellHeight);

  DSpatial spatial{
    .transform = translationMatrix4x4(region.position),
    .parent = sysSpatial.root(),
    .enabled = true,
    .aabb = Aabb{
      .min = { 0.f, minHeight, 0.f },
      .max = { cellWidth, maxHeight, cellHeight }
    }
  };

  sysSpatial.addEntity(id, spatial);

  DModelPtr render = std::make_unique<DModel>();
  render->model = region.model;

  sysRender3d.addEntity(id, std::move(render));

  DTerrainChunk collision{
    .heightMap = region.heightMap
  };

  return { id };
}

MeshPtr TerrainBuilderImpl::constructMesh(const Texture& heightMap) const
{
  // TODO: Break into smaller chunks

  ASSERT(heightMap.channels == 1,
    "Height map has " << heightMap.channels << " channels; expected 1");

  assert(heightMap.data.size() == heightMap.width * heightMap.height);

  float minHeight = metresToWorldUnits(m_config.minHeight);
  float maxHeight = metresToWorldUnits(m_config.maxHeight);
  float cellWidth = metresToWorldUnits(m_config.cellWidth);
  float cellHeight = metresToWorldUnits(m_config.cellHeight);
  float heightRange = maxHeight - minHeight;

  MeshPtr mesh = std::make_unique<Mesh>();

  mesh->attributeBuffers = {
    render::Buffer{
      .usage = BufferUsage::AttrPosition,
      .data = {}
    },
    render::Buffer{
      .usage = BufferUsage::AttrNormal,
      .data = {}
    },
    render::Buffer{
      .usage = BufferUsage::AttrTexCoord,
      .data = {}
    }
  };
  mesh->indexBuffer = render::Buffer{
    .usage = BufferUsage::Index,
    .data = {}
  };

  uint32_t numVertices = heightMap.width * heightMap.height;
  size_t numIndices = 6 * (heightMap.width - 1) * (heightMap.height - 1);

  mesh->attributeBuffers[0].data.resize(numVertices * sizeof(Vec3f));
  mesh->attributeBuffers[1].data.resize(numVertices * sizeof(Vec3f));
  mesh->attributeBuffers[2].data.resize(numVertices * sizeof(Vec2f));

  mesh->indexBuffer.data.resize(numIndices * sizeof(uint16_t));

  auto positions = reinterpret_cast<Vec3f*>(mesh->attributeBuffers[0].data.data());
  auto normals = reinterpret_cast<Vec3f*>(mesh->attributeBuffers[1].data.data());
  auto texCoords = reinterpret_cast<Vec2f*>(mesh->attributeBuffers[2].data.data());

  auto indices = reinterpret_cast<uint16_t*>(mesh->indexBuffer.data.data());

  mesh->featureSet = render::MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags = bitflag(render::MeshFeatures::IsTerrain) | bitflag(render::MeshFeatures::CastsShadow)
  };

  for (uint32_t i = 0; i < numVertices; ++i) {
    uint32_t pixelX = i % heightMap.width;
    uint32_t pixelY = i / heightMap.height;

    float uvX = static_cast<float>(pixelX) / heightMap.width;
    float uvY = static_cast<float>(pixelY) / heightMap.height;
    float x = uvX * cellWidth;
    float z = uvY * cellHeight;

    uint32_t pixelValue = heightMap.data.at(i);
    float height = minHeight + (static_cast<float>(pixelValue) / 255.f) * heightRange;

    positions[i] = { x, height, z };
    texCoords[i] = { uvX, uvY };
  }

  size_t indexIdx = 0;
  for (uint32_t pixelY = 1; pixelY < heightMap.height; ++pixelY) {
    for (uint32_t pixelX = 1; pixelX < heightMap.width; ++pixelX) {
      uint32_t i = pixelY * heightMap.width + pixelX;

      uint32_t A = i - 1;
      uint32_t B = i;
      uint32_t D = i - heightMap.width - 1;
      uint32_t C = D + 1;

      indices[indexIdx++] = A;
      indices[indexIdx++] = B;
      indices[indexIdx++] = C;

      indices[indexIdx++] = A;
      indices[indexIdx++] = C;
      indices[indexIdx++] = D;

      Vec3f AB = positions[B] - positions[A];
      Vec3f AD = positions[D] - positions[A];

      normals[i] = AB.cross(AD).normalise();
    }
  }

  return mesh;
}

// TODO: Cache meshes
ResourceHandle TerrainBuilderImpl::loadTerrainRegionAsync(uint32_t x, uint32_t y,
  XmlNodePtr terrainXml)
{
  auto loader = [this, x, y, terrainXml = std::move(terrainXml)](ResourceId id) mutable {
    const auto cellPath = fs::path{"worlds"} / m_config.world / cellName(x, y);

    auto heightMapTextureData = m_fileSystem.readAppDataFile(cellPath / "height_map.png");
    auto heightMapTexture = render::loadGreyscaleTexture(heightMapTextureData);

    auto mesh = constructMesh(*heightMapTexture);
    HeightMap heightMap;
    heightMap.widthPx = heightMapTexture->width;
    heightMap.heightPx = heightMapTexture->height;
    heightMap.width = m_config.cellWidth;
    heightMap.height = m_config.cellHeight;
    heightMap.data = constructHeightMap(*mesh);

    render::MaterialFeatureSet materialFeatures{
      .flags = bitflag(render::MaterialFeatures::HasTexture)
    };

    auto& splatMapXml = *terrainXml->child("splat-map");

    auto material = std::make_unique<render::Material>();
    material->featureSet = materialFeatures;

    auto splatMapHandle = m_renderResourceLoader.loadTextureAsync(cellPath / "splat_map.png");
    material->textures.push_back(splatMapHandle);

    for (auto& textureXml : splatMapXml) {
      auto filePath = fs::path{"textures"} / textureXml.attribute("file");

      if (!m_renderResourceLoader.hasTexture(filePath)) {
        auto texture = m_renderResourceLoader.loadTextureAsync(filePath);
        material->textures.push_back(texture);
      }
      else {
        auto texture = m_renderResourceLoader.getTextureHandle(filePath);
        material->textures.push_back(texture);
      }
    }

    auto submodel = std::make_unique<Submodel>();
    submodel->mesh = m_renderResourceLoader.loadMeshAsync(std::move(mesh));
    submodel->material = m_renderResourceLoader.loadMaterialAsync(std::move(material));

    auto model = std::make_unique<Model>();
    model->submodels.push_back(std::move(submodel));

    TerrainRegion region;
    region.heightMap = heightMap;
    region.position = metresToWorldUnits(Vec3f{
      x * m_config.cellWidth,
      0.f,
      y * m_config.cellHeight
    });
    region.model = m_modelLoader.loadModelAsync(std::move(model));

    m_regions.insert({ id, std::move(region) });

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        m_regions.erase(id);
      }
    };
  };

  return m_resourceManager.loadResource(std::move(loader));
}

} // namespace

TerrainBuilderPtr createTerrainBuilder(const TerrainConfig& config, Ecs& ecs,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<TerrainBuilderImpl>(config, ecs, modelLoader, renderResourceLoader,
    resourceManager, fileSystem, logger);
}

} // namespace lithic3d
