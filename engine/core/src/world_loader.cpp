#include "lithic3d/world_loader.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/entity_factory.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/terrain_builder.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/units.hpp"
#include <map>
#include <cassert>

namespace fs = std::filesystem;

namespace lithic3d
{
namespace
{

Vec3f constructVec3f(const XmlNode& node)
{
  return {
    std::stof(node.attribute("x")),
    std::stof(node.attribute("y")),
    std::stof(node.attribute("z"))
  };
}

Mat4x4f constructTransform(const XmlNode& transformXml)
{
  auto pos = constructVec3f(*transformXml.child("pos"));
  auto ori = constructVec3f(*transformXml.child("ori"));

  Vec3f scale{ 1.f, 1.f, 1.f };

  auto i = transformXml.child("scale");
  if (i != transformXml.end()) {
    scale = constructVec3f(*i);
  }

  return createTransform(metresToWorldUnits(pos), ori, scale);
}
  
struct CellSlice
{
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t sliceIdx = 0;
  std::vector<ResourceHandle> prefabs;
  std::vector<XmlNodePtr> pendingEntities;
  ResourceHandle terrain;
};

fs::path cellSlicePath(uint32_t x, uint32_t y, uint32_t sliceIdx)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << x
    << std::setw(3) << std::setfill('0') << y;

  fs::path dir{ss.str()};

  ss.str("");
  ss << std::setw(3) << std::setfill('0') << sliceIdx << ".xml";

  return dir / ss.str();
}

class WorldLoaderImpl : public WorldLoader
{
  public:
    WorldLoaderImpl(Ecs& ecs, FileSystem& fileSystem, EntityFactory& entityFactory,
      ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
      ResourceManager& resourceManager, Logger& logger);

    const WorldInfo& worldInfo() const override;
    ResourceHandle loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx) override;
    std::vector<EntityId> createEntities(ResourceId cellSliceId) override;

  private:
    Logger& m_logger;
    FileSystem& m_fileSystem;
    EntityFactory& m_entityFactory;
    ModelLoader& m_modelLoader;
    RenderResourceLoader& m_renderResourceLoader;
    ResourceManager& m_resourceManager;
    TerrainBuilderPtr m_terrainBuilder;
    std::string m_worldName = "world"; // TODO
    WorldInfo m_worldInfo;
    std::map<ResourceId, CellSlice> m_cellSlices;
    std::vector<ResourceId> m_pendingSlices;

    void loadWorldInfo(const XmlNode& node);
    TerrainConfig loadTerrainConfig(const XmlNode& node);
};

WorldLoaderImpl::WorldLoaderImpl(Ecs& ecs, FileSystem& fileSystem, EntityFactory& entityFactory,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_entityFactory(entityFactory)
  , m_modelLoader(modelLoader)
  , m_renderResourceLoader(renderResourceLoader)
  , m_resourceManager(resourceManager)
{
  auto worldFilePath = fs::path{"worlds"} / m_worldName / "world.xml";

  auto xmlData = m_fileSystem.readAppDataFile(worldFilePath);
  auto worldXml = parseXml(xmlData.data());

  loadWorldInfo(*worldXml);

  TerrainConfig terrainConfig{
    .world = m_worldName,
    .minHeight = worldUnitsToMetres(m_worldInfo.minElevation),
    .maxHeight = worldUnitsToMetres(m_worldInfo.maxElevation),
    .cellWidth = worldUnitsToMetres(m_worldInfo.cellWidth),
    .cellHeight = worldUnitsToMetres(m_worldInfo.cellHeight),
  };

  m_terrainBuilder = createTerrainBuilder(terrainConfig, ecs, modelLoader, m_renderResourceLoader,
    m_resourceManager, m_fileSystem, m_logger);
}

std::vector<EntityId> WorldLoaderImpl::createEntities(ResourceId cellSliceId)
{
  auto& cellSlice = m_cellSlices.at(cellSliceId);
  std::vector<EntityId> entities;

  if (cellSlice.terrain) {
    assert(cellSlice.terrain.ready());
    entities = m_terrainBuilder->createEntities(cellSlice.terrain.id());
  }

  for (auto& entityXml : cellSlice.pendingEntities) {
    auto type = entityXml->attribute("type");
    auto transform = constructTransform(*entityXml->child("transform"));

    auto id = m_entityFactory.constructEntity(type, transform);
    entities.push_back(id);
  }

  cellSlice.pendingEntities.clear();

  return entities;
}

const WorldInfo& WorldLoaderImpl::worldInfo() const
{
  return m_worldInfo;
}

ResourceHandle WorldLoaderImpl::loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx)
{ 
  auto handle = m_resourceManager.loadResource([this, x, y, sliceIdx](ResourceId id) {
    const auto worldsPath = fs::path{"worlds"};
    const auto cellSliceFilePath = worldsPath / m_worldName / cellSlicePath(x, y, sliceIdx);

    auto cellSliceXmlFileData = m_fileSystem.readAppDataFile(cellSliceFilePath);
    auto cellSliceXml = parseXml(cellSliceXmlFileData.data());

    ASSERT(cellSliceXml->name() == "cell-slice", "Expected <cell-slice> element");

    auto i = cellSliceXml->child("entities");
    if (i != cellSliceXml->end()) {
      for (auto& entityXml : *i) {
        auto type = entityXml.attribute("type");

        if (!m_entityFactory.hasPrefab(type)) {
          m_cellSlices[id].prefabs.push_back(m_entityFactory.loadPrefabAsync(type));
        }

        m_cellSlices[id].pendingEntities.push_back(entityXml.clone());
      }
    }

    i = cellSliceXml->child("terrain");
    if (i != cellSliceXml->end()) {
      m_cellSlices[id].terrain = m_terrainBuilder->loadTerrainRegionAsync(x, y, i->clone());
    }

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        m_cellSlices.erase(id);
      }
    };
  });

  DBG_ASSERT(!m_cellSlices.contains(handle.id()), "Cell slice already loaded");

  m_cellSlices.insert({ handle.id(), CellSlice{
    .x = x,
    .y = y,
    .sliceIdx = sliceIdx,
    .prefabs = {},
    .pendingEntities = {},
    .terrain = {}
  }});

  return handle;
}

void WorldLoaderImpl::loadWorldInfo(const XmlNode& node)
{
  m_worldInfo = WorldInfo{
    .gridWidth = static_cast<uint32_t>(std::stoi(node.attribute("grid-width"))),
    .gridHeight = static_cast<uint32_t>(std::stoi(node.attribute("grid-height"))),
    .cellWidth = metresToWorldUnits(std::stof(node.attribute("cell-width"))),
    .cellHeight = metresToWorldUnits(std::stof(node.attribute("cell-height"))),
    .minElevation = metresToWorldUnits(std::stof(node.attribute("min-elevation"))),
    .maxElevation = metresToWorldUnits(std::stof(node.attribute("max-elevation")))
  };
}

} // namespace

WorldLoaderPtr createWorldLoader(Ecs& ecs, FileSystem& fileSystem, EntityFactory& entityFactory,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, Logger& logger)
{
  return std::make_unique<WorldLoaderImpl>(ecs, fileSystem, entityFactory, modelLoader,
    renderResourceLoader, resourceManager, logger);
}

} // namespace lithic3d
