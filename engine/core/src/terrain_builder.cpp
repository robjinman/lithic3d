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
using render::Buffer;
using render::AlignedBytes;

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
  size_t n = mesh.attributeBuffers[0].data.numElements();
  auto positions = mesh.attributeBuffers[0].data.data<Vec3f>();

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

  m_ecs.componentStore().allocate<DSpatial, DModel, DTerrainChunk>(id);

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
    .restitution = 0.f,   // TODO
    .friction = 0.4f,     // TODO
    .heightMap = region.heightMap
  };

  sysCollision.addEntity(id, collision);

  return { id };
}

MeshPtr TerrainBuilderImpl::constructMesh(const Texture& heightMap) const
{
  // TODO: Break into smaller chunks

  ASSERT(heightMap.channels == 1,
    "Height map has " << heightMap.channels << " channels; expected 1");

  assert(heightMap.width > 0);
  assert(heightMap.height > 0);
  assert(heightMap.data.size() == heightMap.width * heightMap.height);

  float minHeight = metresToWorldUnits(m_config.minHeight);
  float maxHeight = metresToWorldUnits(m_config.maxHeight);
  float cellWidth = metresToWorldUnits(m_config.cellWidth);
  float cellHeight = metresToWorldUnits(m_config.cellHeight);
  float heightRange = maxHeight - minHeight;

  uint32_t numVertices = heightMap.width * heightMap.height;
  size_t numIndices = 6 * (heightMap.width - 1) * (heightMap.height - 1);

  std::vector<Vec3f> positions(numVertices);
  std::vector<Vec3f> normals(numVertices);
  std::vector<Vec2f> texCoords(numVertices);
  std::vector<uint16_t> indices(numIndices);

  MeshPtr mesh = std::make_unique<Mesh>();

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
    uint32_t pixelY = i / heightMap.width;

    float uvX = static_cast<float>(pixelX) / (heightMap.width - 1);
    float uvY = static_cast<float>(pixelY) / (heightMap.height - 1);
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
      Vec3f AC = positions[C] - positions[A];
      Vec3f AD = positions[D] - positions[A];

      auto abcNormal = AB.cross(AC);
      auto acdNormal = AC.cross(AD);

      normals[A] += abcNormal + acdNormal;
      normals[B] += abcNormal;
      normals[C] += abcNormal + acdNormal;
      normals[D] += acdNormal;
    }
  }

  for (auto& n : normals) {
    n = n.normalise();
  }

  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{positions}, BufferUsage::AttrPosition});
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{normals}, BufferUsage::AttrNormal});
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{texCoords}, BufferUsage::AttrTexCoord});
  mesh->indexBuffer = Buffer{AlignedBytes{indices}, BufferUsage::Index};

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
    heightMap.width = metresToWorldUnits(m_config.cellWidth);
    heightMap.height = metresToWorldUnits(m_config.cellHeight);
    heightMap.data = constructHeightMap(*mesh);

    render::MaterialFeatureSet materialFeatures{
      .flags = bitflag(render::MaterialFeatures::HasTexture)
    };

    auto& splatMapXml = *terrainXml->child("splat-map");

    auto material = std::make_unique<render::Material>();
    material->featureSet = materialFeatures;

    material->splatMap = m_renderResourceLoader.loadTextureAsync(cellPath / "splat_map.png");

    for (auto& textureXml : splatMapXml) {
      auto filePath = fs::path{"textures"} / textureXml.attribute("file");

      ResourceHandle texture;
      if (!m_renderResourceLoader.hasTexture(filePath)) {
        texture = m_renderResourceLoader.loadTextureAsync(filePath);
      }
      else {
        texture = m_renderResourceLoader.getTextureHandle(filePath);
      }
      material->textures.push_back(texture);
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
