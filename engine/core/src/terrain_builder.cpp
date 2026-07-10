#include "lithic3d/terrain_builder.hpp"
#include "lithic3d/renderables.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/game_data_paths.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/units.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/ecs.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_collision.hpp"
#include "lithic3d/model_loader.hpp"
#include "lithic3d/xml_utils.hpp"
#include <cassert>
#include <unordered_map>
#include <mutex>

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

struct TerrainPiece
{
  HeightMap heightMap;
  Vec3f position;     // World units
  Vec3f dimensions;   // y-dimension is max height
  ResourceHandle model;
};

struct Water
{
  Vec3f position;
  ResourceHandle model;
};

struct TerrainRegion
{
  std::vector<TerrainPiece> land;
  Water water;
};

std::string cellName(uint32_t x, uint32_t y)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << x << std::setw(3) << std::setfill('0') << y;
  return ss.str();
}

class TerrainBuilderImpl : public TerrainBuilder
{
  public:
    TerrainBuilderImpl(const Vec2f& cellSizeMetres, Ecs& ecs, ModelLoader& modelLoader,
      RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
      const GameDataPaths& paths, Logger& logger);

    ResourceHandle loadTerrainRegionAsync(uint32_t x, uint32_t y, XmlNodePtr xmlTerrain) override;
    std::vector<EntityId> createEntities(EntityId parentId, ResourceId regionId) override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    ModelLoader& m_modelLoader;
    RenderResourceLoader& m_renderResourceLoader;
    ResourceManager& m_resourceManager;
    const GameDataPaths& m_paths;
    std::string m_worldName = "world"; // TODO
    Vec2f m_cellSizeMetres;
    std::mutex m_mutex;
    std::unordered_map<ResourceId, TerrainRegion> m_regions;

    MeshPtr constructLandMesh(const Texture& heightMap, const Vec3f& dimensions, bool inverted,
      std::vector<float>& heights, std::vector<bool>& mask) const;
    TerrainPiece constructLandModelAsync(const fs::path& cellPath, const XmlNode& xmlTerrain) const;
    MeshPtr constructWaterMesh(const Vec2f& cellSize) const;
    ResourceHandle constructWaterModelAsync(const Vec2f& cellSize) const;
    void createLandEntities(EntityId parentId, const TerrainRegion& region,
      std::vector<EntityId>& entities);
    EntityId createWaterEntity(EntityId parentId, const TerrainRegion& region);
};

TerrainBuilderImpl::TerrainBuilderImpl(const Vec2f& cellSizeMetres, Ecs& ecs,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, const GameDataPaths& paths, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderResourceLoader(renderResourceLoader)
  , m_resourceManager(resourceManager)
  , m_paths(paths)
  , m_cellSizeMetres(cellSizeMetres)
{
}

EntityId TerrainBuilderImpl::createWaterEntity(EntityId parentId, const TerrainRegion& region)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();

  auto id = m_ecs.idGen().getNewEntityId();

  m_ecs.componentStore().allocate<DSpatial, DModel>(id);

  float cellWidth = metresToWorldUnits(m_cellSizeMetres[0]);
  float cellHeight = metresToWorldUnits(m_cellSizeMetres[1]);

  float waterSurfaceThickness = metresToWorldUnits(0.5f);

  DSpatial spatial{
    .transform = translationMatrix4x4(region.water.position),
    .parent = parentId,
    .enabled = true,
    .aabb = Aabb{
      .min = { 0.f, -0.5f * waterSurfaceThickness, 0.f },
      .max = { cellWidth, 0.5f * waterSurfaceThickness, cellHeight }
    }
  };

  sysSpatial.addEntity(id, spatial);

  DModelPtr render = std::make_unique<DModel>();
  render->model = region.water.model;

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

void TerrainBuilderImpl::createLandEntities(EntityId parentId, const TerrainRegion& region,
  std::vector<EntityId>& entities)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();
  auto& sysCollision = m_ecs.system<SysCollision>();

  for (auto& piece : region.land) {
    auto id = m_ecs.idGen().getNewEntityId();
    m_ecs.componentStore().allocate<DSpatial, DModel, DTerrainChunk>(id);

    DSpatial spatial{
      .transform = translationMatrix4x4(piece.position),
      .parent = parentId,
      .enabled = true,
      .aabb = Aabb{
        .min = { 0.f, 0.f, 0.f },
        .max = piece.dimensions
      }
    };

    sysSpatial.addEntity(id, spatial);

    DModelPtr render = std::make_unique<DModel>();
    render->model = piece.model;

    sysRender3d.addEntity(id, std::move(render));

    DTerrainChunk collision{
      .restitution = 0.1f,   // TODO
      .friction = 0.5f,     // TODO
      // TODO: This copies the height map. Use pointer instead?
      // Alternatively, clear piece.heightMap?
      .heightMap = piece.heightMap
    };

    sysCollision.addEntity(id, collision);

    entities.push_back(id);
  }
}

std::vector<EntityId> TerrainBuilderImpl::createEntities(EntityId parentId, ResourceId regionId)
{
  TerrainRegion* region = nullptr;

  {
    std::scoped_lock lock{m_mutex};
    region = &m_regions.at(regionId);
  }

  std::vector<EntityId> entities;
  entities.push_back(createWaterEntity(parentId, *region));
  createLandEntities(parentId, *region, entities);

  return entities;
}

MeshPtr TerrainBuilderImpl::constructLandMesh(const Texture& heightMap, const Vec3f& dimensions,
  bool inverted, std::vector<float>& heights, std::vector<bool>& mask) const
{
  // TODO: Break into smaller chunks

  ASSERT(heightMap.channels == 1,
    "Height map has " << heightMap.channels << " channels; expected 1");

  assert(heightMap.width > 0);
  assert(heightMap.height > 0);
  assert(heightMap.data.size() == heightMap.width * heightMap.height);

  uint32_t numVertices = heightMap.width * heightMap.height;
  size_t numIndices = 6 * (heightMap.width - 1) * (heightMap.height - 1);

  std::vector<Vec3f> positions(numVertices);
  std::vector<Vec3f> normals(numVertices);
  std::vector<Vec2f> texCoords(numVertices);
  std::vector<uint16_t> indices(numIndices);

  heights.resize(numVertices);
  mask.resize(numVertices);

  MeshPtr mesh = std::make_unique<Mesh>();

  mesh->featureSet = render::MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags = bitflag(render::MeshFeatures::IsTerrain) | bitflag(render::MeshFeatures::CastsShadow)
  };

  auto calcHeight = [dimensions, inverted](uint32_t pixelValue) {
    if (inverted) {
      pixelValue = 255 - pixelValue;
    }
    return (static_cast<float>(pixelValue) / 255.f) * dimensions[1];
  };

  for (uint32_t i = 0; i < numVertices; ++i) {
    uint32_t pixelX = i % heightMap.width;
    uint32_t pixelY = i / heightMap.width;

    float uvX = static_cast<float>(pixelX) / (heightMap.width - 1);
    float uvY = static_cast<float>(pixelY) / (heightMap.height - 1);
    float x = uvX * dimensions[0];
    float z = uvY * dimensions[2];

    float height = calcHeight(heightMap.data.at(i));

    positions[i] = { x, height, z };
    texCoords[i] = { uvX, uvY };

    heights[i] = metresToWorldUnits(height);
    mask[i] = true;
  }

  size_t indexIdx = 0;
  for (uint32_t pixelY = 1; pixelY < heightMap.height; ++pixelY) {
    for (uint32_t pixelX = 1; pixelX < heightMap.width; ++pixelX) {
      uint32_t i = pixelY * heightMap.width + pixelX;

      uint32_t A = i - 1;
      uint32_t B = i;
      uint32_t D = i - heightMap.width - 1;
      uint32_t C = D + 1;

      uint32_t Ay = heightMap.data.at(A);
      uint32_t By = heightMap.data.at(B);
      uint32_t Cy = heightMap.data.at(C);
      uint32_t Dy = heightMap.data.at(D);

      if (Ay == 0 && By == 0 && Cy == 0 && Dy == 0) {
        uint32_t E = A + heightMap.width;
        uint32_t F = E + 1;
        uint32_t G = B + 1;
        uint32_t H = C + 1;
        uint32_t I = C - heightMap.width;
        uint32_t J = I - 1;
        uint32_t K = D - 1;
        uint32_t L = A - 1;

        positions[A][1] = 0.5f * (positions[E][1] + positions[L][1]);
        positions[B][1] = 0.5f * (positions[F][1] + positions[G][1]);
        positions[C][1] = 0.5f * (positions[H][1] + positions[I][1]);
        positions[D][1] = 0.5f * (positions[J][1] + positions[K][1]);

        heights[A] = metresToWorldUnits(positions[A][1]);
        heights[B] = metresToWorldUnits(positions[B][1]);
        heights[C] = metresToWorldUnits(positions[C][1]);
        heights[D] = metresToWorldUnits(positions[D][1]);

        mask[A] = false;
        mask[B] = false;
        mask[C] = false;
        mask[D] = false;

        // TODO: Remove vertex data?
      }
      else {
        if (inverted) {
          indices[indexIdx++] = C;
          indices[indexIdx++] = B;
          indices[indexIdx++] = A;

          indices[indexIdx++] = D;
          indices[indexIdx++] = C;
          indices[indexIdx++] = A;
        }
        else {
          indices[indexIdx++] = A;
          indices[indexIdx++] = B;
          indices[indexIdx++] = C;

          indices[indexIdx++] = A;
          indices[indexIdx++] = C;
          indices[indexIdx++] = D;
        }
      }

      Vec3f AB = positions[B] - positions[A];
      Vec3f AC = positions[C] - positions[A];
      Vec3f AD = positions[D] - positions[A];

      if (Ay != 0 && Cy != 0) {
        auto abcNormal = AB.cross(AC);
        auto acdNormal = AC.cross(AD);

        if (By != 0 && Dy != 0) {
          normals[A] += abcNormal + acdNormal;
          normals[C] += abcNormal + acdNormal;
        }
        if (By != 0) {
          normals[B] += abcNormal;
        }
        if (Dy != 0) {
          normals[D] += acdNormal;
        }
      }
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

MeshPtr TerrainBuilderImpl::constructWaterMesh(const Vec2f& cellSize) const
{
  MeshPtr mesh = std::make_unique<Mesh>();

  mesh->featureSet = render::MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,  // TODO: Remove?
      BufferUsage::AttrTexCoord // TODO: Remove?
    },
    .flags = 0
  };

  std::vector<Vec3f> positions{
    Vec3f{ 0.f, 0.f, 0.f },
    Vec3f{ cellSize[0], 0.f, 0.f },
    Vec3f{ cellSize[0], 0.f, cellSize[1] },
    Vec3f{ 0.f, 0.f, cellSize[1] }
  };

  std::vector<Vec3f> normals{
    Vec3f{ 0.f, 1.f, 0.f },
    Vec3f{ 0.f, 1.f, 0.f },
    Vec3f{ 0.f, 1.f, 0.f },
    Vec3f{ 0.f, 1.f, 0.f }
  };

  std::vector<Vec2f> texCoords
  {
    Vec2f{ 0.f, 0.f },
    Vec2f{ 1.f, 0.f },
    Vec2f{ 1.f, 1.f },
    Vec2f{ 0.f, 1.f }
  };

  std::vector<uint16_t> indices{
    0, 3, 1,
    3, 2, 1
  };

  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{positions}, BufferUsage::AttrPosition});
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{normals}, BufferUsage::AttrNormal});
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{texCoords}, BufferUsage::AttrTexCoord});
  mesh->indexBuffer = Buffer{AlignedBytes{indices}, BufferUsage::Index};

  return mesh;
}

TerrainPiece TerrainBuilderImpl::constructLandModelAsync(const fs::path& cellPath,
  const XmlNode& xmlTerrainPiece) const
{
  // TODO: Read file name from floor_height_map attribute

  auto dimensionsMetres = constructVec3f(*xmlTerrainPiece.child("dim"));

  bool inverted = xmlTerrainPiece.attribute("inverted") == "true";

  TerrainPiece piece;
  piece.position = metresToWorldUnits(constructVec3f(*xmlTerrainPiece.child("pos")));
  piece.dimensions = metresToWorldUnits(dimensionsMetres);

  auto heightMapFilename = xmlTerrainPiece.attribute("height_map");

  auto heightMapTextureData = m_paths.worldsDir->readFile(cellPath / heightMapFilename);
  auto heightMapTexture = render::loadGreyscaleTexture(heightMapTextureData);

  piece.heightMap.inverted = inverted;
  piece.heightMap.widthPx = heightMapTexture->width;
  piece.heightMap.heightPx = heightMapTexture->height;
  piece.heightMap.width = piece.dimensions[0];
  piece.heightMap.height = piece.dimensions[2];
  auto mesh = constructLandMesh(*heightMapTexture, dimensionsMetres, inverted, piece.heightMap.data,
    piece.heightMap.mask);

  render::MaterialFeatureSet materialFeatures{
    .flags = bitflag(render::MaterialFeatures::HasTexture)
  };

  auto& xmlSplatMap = *xmlTerrainPiece.child("splat_map");
  auto splatMapFilename = xmlSplatMap.attribute("file");

  auto material = std::make_unique<render::Material>();
  material->featureSet = materialFeatures;
  material->splatMap = m_renderResourceLoader.loadTextureAsync(cellPath / splatMapFilename, true,
    m_paths.worldsDir);

  for (auto& textureXml : xmlSplatMap) {
    fs::path filePath = textureXml.attribute("file");
    material->textures.push_back(m_renderResourceLoader.loadTextureAsync(filePath, true));
  }

  // TODO: Multiple terrain LODs

  auto submodel = std::make_unique<Submodel>();
  submodel->lods = { m_renderResourceLoader.loadMeshAsync(std::move(mesh)) };
  submodel->material = m_renderResourceLoader.loadMaterialAsync(std::move(material));

  auto model = std::make_unique<Model>();
  model->submodels.push_back(std::move(submodel));

  piece.model = m_modelLoader.loadModelAsync(std::move(model));

  return piece;
}

ResourceHandle TerrainBuilderImpl::constructWaterModelAsync(const Vec2f& cellSize) const
{
  auto material = std::make_unique<render::Material>();
  material->featureSet = {
    .flags = bitflag(render::MaterialFeatures::IsWater)
  };
  material->colour = { 0.15f, 0.2f, 0.6f, 1.f };

  auto mesh = constructWaterMesh(cellSize);

  auto submodel = std::make_unique<Submodel>();
  submodel->lods = { m_renderResourceLoader.loadMeshAsync(std::move(mesh)) };
  submodel->material = m_renderResourceLoader.loadMaterialAsync(std::move(material));

  auto model = std::make_unique<Model>();
  model->submodels.push_back(std::move(submodel));

  return m_modelLoader.loadModelAsync(std::move(model));
}

// TODO: Cache meshes
ResourceHandle TerrainBuilderImpl::loadTerrainRegionAsync(uint32_t x, uint32_t y,
  XmlNodePtr xmlTerrain)
{
  auto loader = [this, x, y, xmlTerrain = std::move(xmlTerrain)](ResourceId id) mutable {
    const auto cellPath = fs::path{m_worldName} / cellName(x, y);

    Vec2f cellSizeWorld = metresToWorldUnits(m_cellSizeMetres);

    TerrainRegion region;

    for (auto& xmlTerrainPiece : *xmlTerrain) {
      region.land.push_back(constructLandModelAsync(cellPath, xmlTerrainPiece));
    }

    float waterLevel = metresToWorldUnits(std::stof(xmlTerrain->attribute("water_level")));

    region.water = {
      .position = { x * cellSizeWorld[0], waterLevel, y * cellSizeWorld[1] },
      .model = constructWaterModelAsync(m_cellSizeMetres)
    };

    {
      std::scoped_lock lock{m_mutex};
      m_regions.insert({ id, std::move(region) });
    }

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        std::scoped_lock lock{m_mutex};
        m_regions.erase(id);
      }
    };
  };

  return m_resourceManager.loadResource(std::move(loader));
}

} // namespace

TerrainBuilderPtr createTerrainBuilder(const Vec2f& cellSizeMetres, Ecs& ecs,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, const GameDataPaths& paths, Logger& logger)
{
  return std::make_unique<TerrainBuilderImpl>(cellSizeMetres, ecs, modelLoader,
    renderResourceLoader, resourceManager, paths, logger);
}

} // namespace lithic3d
