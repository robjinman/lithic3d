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
#include "lithic3d/scoped_lock.hpp"
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
    const TerrainPiece& getTerrainPiece(EntityId id) const override;

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

    MeshPtr constructLandMesh(const Texture& heightMap, const Recti& rect, const Vec3f& dimensions,
      bool inverted, std::vector<float>& heights, std::vector<bool>& mask, float& maxHeight) const;
    TerrainPiece constructTerrainPieceAsync(const fs::path& cellPath,
      const XmlNode& xmlTerrain) const;
    MeshPtr constructWaterMesh(const Vec2f& cellSize) const;
    ResourceHandle constructWaterModelAsync(const Vec2f& cellSize) const;
    void createLandEntities(EntityId parentId, TerrainRegion& region,
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

const TerrainPiece& TerrainBuilderImpl::getTerrainPiece(EntityId id) const
{
  // Naive search

  for (auto& entry : m_regions) {
    for (auto& piece : entry.second.land) {
      if (piece.entityId == id) {
        return piece;
      }
    }
  }

  EXCEPTION(STR("No terrain piece with entity ID " << id));
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

void TerrainBuilderImpl::createLandEntities(EntityId parentId, TerrainRegion& region,
  std::vector<EntityId>& entities)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();
  auto& sysCollision = m_ecs.system<SysCollision>();

  for (auto& piece : region.land) {
    auto pieceId = m_ecs.idGen().getNewEntityId();
    m_ecs.componentStore().allocate<DSpatial>(pieceId);

    DSpatial spatial{
      .transform = translationMatrix4x4(piece.position),
      .parent = parentId,
      .enabled = true,
      .aabb = Aabb{
        .min = { 0.f, 0.f, 0.f },
        .max = piece.dimensions
      }
    };

    sysSpatial.addEntity(pieceId, spatial);

    for (auto& chunk : piece.chunks) {
      auto chunkId = m_ecs.idGen().getNewEntityId();
      m_ecs.componentStore().allocate<DSpatial, DModel, DTerrainChunk>(chunkId);

      DSpatial pieceSpatial{
        .transform = translationMatrix4x4(chunk.position),
        .parent = pieceId,
        .enabled = true,
        .aabb = Aabb{
          .min = { 0.f, 0.f, 0.f },
          .max = chunk.dimensions
        }
      };

      sysSpatial.addEntity(chunkId, pieceSpatial);

      DModelPtr render = std::make_unique<DModel>();
      render->model = chunk.model;

      sysRender3d.addEntity(chunkId, std::move(render));

      DTerrainChunk collision{
        .restitution = 0.1f,   // TODO
        .friction = 0.5f,     // TODO
        // TODO: This copies the height map. Use pointer instead?
        // Alternatively, clear piece.heightMap?
        .heightMap = chunk.heightMap
      };

      sysCollision.addEntity(chunkId, collision);
    }

    piece.entityId = pieceId;
    entities.push_back(pieceId);
  }
}

// First entity ID is water, the rest are land
std::vector<EntityId> TerrainBuilderImpl::createEntities(EntityId parentId, ResourceId regionId)
{
  TerrainRegion* region = nullptr;

  {
    SCOPED_LOCK(m_mutex);
    region = &m_regions.at(regionId);
  }

  std::vector<EntityId> entities;
  entities.push_back(createWaterEntity(parentId, *region));
  createLandEntities(parentId, *region, entities);

  return entities;
}

MeshPtr TerrainBuilderImpl::constructLandMesh(const Texture& heightMap,
  const Recti& rect, const Vec3f& dimensions, bool inverted, std::vector<float>& heights,
  std::vector<bool>& mask, float& outMaxHeight) const
{
  ASSERT(heightMap.channels == 1,
    "Height map has " << heightMap.channels << " channels; expected 1");

  assert(heightMap.data.size() == heightMap.width * heightMap.height);
  assert(rect.w <= heightMap.width);
  assert(rect.h <= heightMap.height);
  assert(rect.x + rect.w <= heightMap.width);
  assert(rect.y + rect.h <= heightMap.height);

  uint32_t numVertices = rect.w * rect.h;
  size_t numIndices = 6 * (rect.w - 1) * (rect.h - 1);

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

  float w_rp = 1.f / (heightMap.width - 1);
  float d_rp = 1.f / (heightMap.height - 1);
  float dx = dimensions[0] * w_rp;
  float dz = dimensions[2] * d_rp;

  float maxHeight = std::numeric_limits<float>::lowest();

  for (uint32_t i = 0; i < numVertices; ++i) {
    uint32_t chunkPxX = i % rect.w;
    uint32_t chunkPxZ = i / rect.w;
    uint32_t globalPxX = rect.x + chunkPxX;
    uint32_t globalPxZ = rect.y + chunkPxZ;
    float chunkX = dx * chunkPxX;
    float chunkZ = dz * chunkPxZ;

    float height = calcHeight(heightMap.data.at(globalPxZ * heightMap.width + globalPxX));
    if (height > maxHeight) {
      maxHeight = height;
    }

    positions[i] = { chunkX, height, chunkZ };
    texCoords[i] = { w_rp * globalPxX, d_rp * globalPxZ };

    heights[i] = metresToWorldUnits(height);
    mask[i] = true;
  }

  auto chunkToGlobalIndex = [&rect, &heightMap](uint32_t chunkIdx) {
    uint32_t chunkPxX = chunkIdx % rect.w;
    uint32_t chunkPxZ = chunkIdx / rect.w;
    return (rect.y + chunkPxZ) * heightMap.width + (rect.x + chunkPxX);
  };

  size_t indexIdx = 0;
  for (uint32_t chunkPxZ = 1; chunkPxZ < rect.h; ++chunkPxZ) {
    for (uint32_t chunkPxX = 1; chunkPxX < rect.w; ++chunkPxX) {
      uint32_t i = chunkPxZ * rect.w + chunkPxX;

      uint32_t A = i - 1;
      uint32_t B = i;
      uint32_t D = i - rect.w - 1;
      uint32_t C = D + 1;

      uint32_t Ay = heightMap.data.at(chunkToGlobalIndex(A));
      uint32_t By = heightMap.data.at(chunkToGlobalIndex(B));
      uint32_t Cy = heightMap.data.at(chunkToGlobalIndex(C));
      uint32_t Dy = heightMap.data.at(chunkToGlobalIndex(D));

      // TODO: Consider holes that cross chunk boundaries
      if (Ay == 0 && By == 0 && Cy == 0 && Dy == 0) {
        uint32_t E = A + rect.w;
        uint32_t F = E + 1;
        uint32_t G = B + 1;
        uint32_t H = C + 1;
        uint32_t I = C - rect.w;
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

  outMaxHeight = maxHeight;

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

TerrainPiece TerrainBuilderImpl::constructTerrainPieceAsync(const fs::path& cellPath,
  const XmlNode& xmlTerrainPiece) const
{
  // TODO: Magic numbers. Parameterise.
  const uint32_t chunkPxW = 20;
  const uint32_t chunkPxD = 20;

  TerrainPiece piece;
  piece.inverted = xmlTerrainPiece.attribute("inverted") == "true";
  piece.heightMapFile = xmlTerrainPiece.attribute("height_map");

  auto pieceDimensionsMetres = constructVec3f(*xmlTerrainPiece.child("dim"));

  piece.position = metresToWorldUnits(constructVec3f(*xmlTerrainPiece.child("pos")));
  piece.dimensions = metresToWorldUnits(pieceDimensionsMetres);

  auto heightMapTextureData = m_paths.worldsDir->readFile(cellPath / piece.heightMapFile);
  auto heightMapTexture = render::loadGreyscaleTexture(heightMapTextureData);

  uint32_t heightMapW = heightMapTexture->width;
  uint32_t heightMapD = heightMapTexture->height;

  uint32_t nChunksX = heightMapW / chunkPxW;
  uint32_t nChunksZ = heightMapD / chunkPxD;
  uint32_t lastChunkPxW = chunkPxW;
  uint32_t lastChunkPxD = chunkPxD;
  if (heightMapW % chunkPxW != 0) {
    ++nChunksX;
    lastChunkPxW = heightMapW % chunkPxW;
  }
  if (heightMapD % chunkPxD != 0) {
    ++nChunksZ;
    lastChunkPxD = heightMapD % chunkPxD;
  }

  auto& xmlSplatMap = *xmlTerrainPiece.child("splat_map");
  piece.splatMapFile = xmlSplatMap.attribute("file");

  render::MaterialFeatureSet materialFeatures{
    .flags = bitflag(render::MaterialFeatures::HasTexture)
  };

  auto material = std::make_unique<render::Material>();
  material->featureSet = materialFeatures;
  material->splatMap = m_renderResourceLoader.loadTextureAsync(cellPath / piece.splatMapFile, true,
    m_paths.worldsDir);

  int i = 0;
  for (auto& textureXml : xmlSplatMap) {
    ASSERT(i < 4, "Too many splat textures");

    fs::path filePath = textureXml.attribute("file");
    piece.splatTextures[i++] = filePath;
    material->textures.push_back(m_renderResourceLoader.loadTextureAsync(filePath, true));
  }

  auto materialHandle = m_renderResourceLoader.loadMaterialAsync(std::move(material));

  for (uint32_t i = 0; i < nChunksX; ++i) {
    for (uint32_t j = 0; j < nChunksZ; ++j) {
      Recti rect{
        .x = i * chunkPxW,
        .y = j * chunkPxD,
        .w = i + 1 == nChunksX ? lastChunkPxW : chunkPxW + 1,
        .h = j + 1 == nChunksZ ? lastChunkPxD : chunkPxD + 1,
      };

      float x = (piece.dimensions[0] * rect.x) / (heightMapW - 1);
      float z = (piece.dimensions[2] * rect.y) / (heightMapD - 1);

      TerrainChunk chunk;

      float maxHeightMetres = 0.f;
      auto mesh = constructLandMesh(*heightMapTexture, rect, pieceDimensionsMetres, piece.inverted,
        chunk.heightMap.data, chunk.heightMap.mask, maxHeightMetres);

      chunk.position = { x, 0.f, z };
      chunk.dimensions[0] = (piece.dimensions[0] * (rect.w - 1)) / (heightMapW - 1);
      chunk.dimensions[1] = metresToWorldUnits(maxHeightMetres);
      chunk.dimensions[2] = (piece.dimensions[2] * (rect.h - 1)) / (heightMapD - 1);
      chunk.heightMap.inverted = piece.inverted;
      chunk.heightMap.widthPx = rect.w;
      chunk.heightMap.heightPx = rect.h;
      chunk.heightMap.width = chunk.dimensions[0];
      chunk.heightMap.height = chunk.dimensions[2];

      // TODO: Multiple terrain LODs

      auto submodel = std::make_unique<Submodel>();
      submodel->lods = { m_renderResourceLoader.loadMeshAsync(std::move(mesh)) };
      submodel->material = materialHandle;

      auto model = std::make_unique<Model>();
      model->submodels.push_back(std::move(submodel));

      chunk.model = m_modelLoader.loadModelAsync(std::move(model));

      piece.chunks.push_back(chunk);
    }
  }

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
      region.land.push_back(constructTerrainPieceAsync(cellPath, xmlTerrainPiece));
    }

    float waterLevel = metresToWorldUnits(std::stof(xmlTerrain->attribute("water_level")));

    region.water = {
      .position = { x * cellSizeWorld[0], waterLevel, y * cellSizeWorld[1] },
      .model = constructWaterModelAsync(m_cellSizeMetres)
    };

    {
      SCOPED_LOCK(m_mutex);
      m_regions.insert({ id, std::move(region) });
    }

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        SCOPED_LOCK(m_mutex);
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
