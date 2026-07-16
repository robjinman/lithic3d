#include "lithic3d/world_loader.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/game_data_paths.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/terrain_builder.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/units.hpp"
#include "lithic3d/ecs.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/scoped_lock.hpp"
#include <map>
#include <cassert>
#include <mutex>

namespace fs = std::filesystem;

namespace lithic3d
{
namespace
{

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
    WorldLoaderImpl(Ecs& ecs, const GameDataPaths& paths, EntityFactory& entityFactory,
      ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
      ResourceManager& resourceManager, Logger& logger);

    const WorldInfo& worldInfo() const override;
    EntityId root() const override;
    TerrainBuilder& terrainBuilder() const override;
    ResourceHandle loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx) override;
    std::vector<EntityInfo> createEntities(ResourceId cellSliceId) override;

  private:
    Logger& m_logger;
    const GameDataPaths& m_paths;
    Ecs& m_ecs;
    EntityId m_root = NULL_ENTITY_ID;
    EntityFactory& m_entityFactory;
    ResourceManager& m_resourceManager;
    TerrainBuilderPtr m_terrainBuilder;
    std::string m_worldName = "world"; // TODO
    WorldInfo m_worldInfo;
    mutable std::mutex m_mutex;
    std::map<ResourceId, CellSlice> m_cellSlices;
    std::vector<ResourceId> m_pendingSlices;

    void loadWorldInfo(const XmlNode& node);
    void constructRoot();
};

WorldLoaderImpl::WorldLoaderImpl(Ecs& ecs, const GameDataPaths& paths, EntityFactory& entityFactory,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, Logger& logger)
  : m_logger(logger)
  , m_paths(paths)
  , m_ecs(ecs)
  , m_entityFactory(entityFactory)
  , m_resourceManager(resourceManager)
{
  constructRoot();

  auto worldFilePath = fs::path{m_worldName} / "world.xml";

  auto xmlData = m_paths.worldsDir->readFile(worldFilePath);
  auto worldXml = parseXml(xmlData);

  loadWorldInfo(*worldXml);

  Vec2f cellSize = worldUnitsToMetres(Vec2f{ m_worldInfo.cellWidth, m_worldInfo.cellHeight });

  m_terrainBuilder = createTerrainBuilder(cellSize, m_ecs, modelLoader, renderResourceLoader,
    m_resourceManager, m_paths, m_logger);
}

void WorldLoaderImpl::constructRoot()
{
  m_root = m_ecs.idGen().getNewEntityId();
  m_ecs.componentStore().allocate<DSpatial>(m_root);

  auto& sysSpatial = m_ecs.system<SysSpatial>();

  sysSpatial.addEntity(m_root, DSpatial{
    .transform = identityMatrix<4>(),
    .parent = sysSpatial.root(),
    .enabled = true,
    .aabb{}
  });
}

TerrainBuilder& WorldLoaderImpl::terrainBuilder() const
{
  return *m_terrainBuilder;
}

EntityId WorldLoaderImpl::root() const
{
  return m_root;
}

std::vector<EntityInfo> WorldLoaderImpl::createEntities(ResourceId cellSliceId)
{
  CellSlice* cellSlice = nullptr;
  {
    SCOPED_LOCK(m_mutex);
    cellSlice = &m_cellSlices.at(cellSliceId);
  }

  std::vector<EntityInfo> entities;

  if (cellSlice->terrain) {
    assert(cellSlice->terrain.ready());
    auto terrainIds = m_terrainBuilder->createEntities(m_root, cellSlice->terrain.id());
    for (size_t i = 0; i < terrainIds.size(); ++i) {
      EntityInfo info;
      info.id = terrainIds[i];
      info.type = i == 0 ? "water" : "terrain";
      entities.push_back(std::move(info));
    }
  }

  for (auto& entityXml : cellSlice->pendingEntities) {
    auto type = entityXml->attribute("type");

    EntityInfo info;
    info.id = m_entityFactory.constructEntity(m_root, *entityXml, info.changedFromPrefab,
      info.unused);
    info.type = type;
    entities.push_back(std::move(info));
  }

  cellSlice->pendingEntities.clear();

  return entities;
}

const WorldInfo& WorldLoaderImpl::worldInfo() const
{
  return m_worldInfo;
}

ResourceHandle WorldLoaderImpl::loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx)
{ 
  auto handle = m_resourceManager.loadResource([this, x, y, sliceIdx](ResourceId id) {
    const auto cellSliceFilePath = fs::path{m_worldName} / cellSlicePath(x, y, sliceIdx);

    auto cellSliceXmlFileData = m_paths.worldsDir->readFile(cellSliceFilePath);
    auto cellSliceXml = parseXml(cellSliceXmlFileData);

    ASSERT(cellSliceXml->name() == "cell-slice", "Expected <cell-slice> element");

    CellSlice* cellSlice = nullptr;
    {
      SCOPED_LOCK(m_mutex);
      cellSlice = &m_cellSlices[id];
    }

    auto i = cellSliceXml->child("entities");
    if (i != cellSliceXml->end()) {
      for (auto& entityXml : *i) {
        auto type = entityXml.attribute("type");

        if (!m_entityFactory.hasPrefab(type)) {
          cellSlice->prefabs.push_back(m_entityFactory.loadPrefabAsync(type));
        }

        cellSlice->pendingEntities.push_back(entityXml.clone());
      }
    }

    i = cellSliceXml->child("terrain");
    if (i != cellSliceXml->end()) {
      cellSlice->terrain = m_terrainBuilder->loadTerrainRegionAsync(x, y, i->clone());
    }

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        SCOPED_LOCK(m_mutex);
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
    .gridWidth = static_cast<uint32_t>(std::stoi(node.attribute("grid_width"))),
    .gridHeight = static_cast<uint32_t>(std::stoi(node.attribute("grid_height"))),
    .cellWidth = metresToWorldUnits(std::stof(node.attribute("cell_width"))),
    .cellHeight = metresToWorldUnits(std::stof(node.attribute("cell_height")))
  };
}

} // namespace

WorldLoaderPtr createWorldLoader(Ecs& ecs, const GameDataPaths& paths, EntityFactory& entityFactory,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, Logger& logger)
{
  return std::make_unique<WorldLoaderImpl>(ecs, paths, entityFactory, modelLoader,
    renderResourceLoader, resourceManager, logger);
}

} // namespace lithic3d
