#include "lithic3d/world_loader.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/entity_factory.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/terrain_builder.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/strings.hpp"

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
    auto scale = constructVec3f(*i);
  }

  return createTransform(pos, ori, scale);
}
  
struct CellSlice
{
  std::vector<ResourceHandle> prefabs;
  std::vector<XmlNodePtr> pendingEntities;
};

std::string cellSliceFileName(uint32_t x, uint32_t y, uint32_t sliceIdx)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << x << y << sliceIdx << ".xml";

  return ss.str();
}

class WorldLoaderImpl : public WorldLoader
{
  public:
    WorldLoaderImpl(FileSystem& fileSystem, EntityFactory& entityFactory,
      RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager);

    const WorldInfo& worldInfo() const override;
    ResourceHandle loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx) override;
    void createEntities(ResourceId cellSliceId) override;

  private:
    FileSystem& m_fileSystem;
    EntityFactory& m_entityFactory;
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

WorldLoaderImpl::WorldLoaderImpl(FileSystem& fileSystem, EntityFactory& entityFactory,
  RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager)
  : m_fileSystem(fileSystem)
  , m_entityFactory(entityFactory)
  , m_renderResourceLoader(renderResourceLoader)
  , m_resourceManager(resourceManager)
{
  auto worldFilePath = fs::path{"worlds"} / m_worldName / "world.xml";

  auto xmlData = m_fileSystem.readAppDataFile(worldFilePath);
  auto worldXml = parseXml(xmlData);

  loadWorldInfo(*worldXml);
  auto terrainConfig = loadTerrainConfig(*worldXml->child("terrain"));
  m_terrainBuilder = createTerrainBuilder(terrainConfig);
}

void WorldLoaderImpl::createEntities(ResourceId cellSliceId)
{
  auto& cellSlice = m_cellSlices.at(cellSliceId);

  for (auto& entityXml : cellSlice.pendingEntities) {
    auto type = entityXml->attribute("type");
    auto transform = constructTransform(*entityXml->child("transform"));

    m_entityFactory.constructEntity(type, transform);
  }

  m_cellSlices.clear();
}

const WorldInfo& WorldLoaderImpl::worldInfo() const
{
  return m_worldInfo;
}

ResourceHandle WorldLoaderImpl::loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx)
{ 
  auto handle = m_resourceManager.loadResource([this, x, y, sliceIdx](ResourceId id) {
    const auto worldsPath = fs::path{"worlds"};
    const auto cellSliceFilePath = worldsPath / m_worldName / cellSliceFileName(x, y, sliceIdx);

    auto cellSliceXmlFileData = m_fileSystem.readAppDataFile(cellSliceFilePath);
    auto cellSliceXml = parseXml(cellSliceXmlFileData);

    ASSERT(cellSliceXml->name() == "cell-slice", "Expected <cell-slice> element");

    CellSlice cellSlice;

    for (auto& entityXml : *cellSliceXml) {
      auto type = entityXml.attribute("type");

      if (!m_entityFactory.hasPrefab(type)) {
        m_cellSlices[id].prefabs.push_back(m_entityFactory.loadPrefabAsync(type));
      }

      cellSlice.pendingEntities.push_back(entityXml.clone());
    }

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        m_cellSlices.erase(id);
      }
    };
  });

  DBG_ASSERT(m_cellSlices.contains(handle.id()), "Cell slice already loaded");

  m_cellSlices.insert({ handle.id(), CellSlice{
    .prefabs = {},
    .pendingEntities = {}
  }});

  return handle;
}

TerrainConfig WorldLoaderImpl::loadTerrainConfig(const XmlNode& terrainXml)
{
  auto heightMap = terrainXml.attribute("height-map");
  auto heightMapTextureData = m_fileSystem.readAppDataFile(fs::path{"textures"} / heightMap);

  // TODO
  return TerrainConfig{
  };
}

void WorldLoaderImpl::loadWorldInfo(const XmlNode& node)
{
  m_worldInfo = WorldInfo{
    .gridWidth = static_cast<uint32_t>(std::stoi(node.attribute("grid-width"))),
    .gridHeight = static_cast<uint32_t>(std::stoi(node.attribute("grid-height"))),
    .cellWidth = std::stof(node.attribute("cell-width")),
    .cellHeight = std::stof(node.attribute("cell-height"))
  };
}

} // namespace

WorldLoaderPtr createWorldLoader(FileSystem& fileSystem, EntityFactory& entityFactory,
  RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager)
{
  return std::make_unique<WorldLoaderImpl>(fileSystem, entityFactory, renderResourceLoader,
    resourceManager);
}

} // namespace lithic3d
