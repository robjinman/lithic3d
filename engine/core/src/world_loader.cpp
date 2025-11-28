#include "lithic3d/world_loader.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/entity_factory.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/terrain_builder.hpp"

namespace fs = std::filesystem;

namespace lithic3d
{
namespace
{

class WorldLoaderImpl : public WorldLoader, private ResourceProvider
{
  public:
    WorldLoaderImpl(FileSystem& fileSystem, EntityFactory& entityFactory,
      ResourceManager& resourceManager);

    const WorldInfo& worldInfo() const override;
    ResourceHandle loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx) override;

  private:
    FileSystem& m_fileSystem;
    EntityFactory& m_entityFactory;
    ResourceManager& m_resourceManager;
    TerrainBuilderPtr m_terrainBuilder;
    std::string m_worldName = "world1"; // TODO
    WorldInfo m_worldInfo;

    void loadWorldInfo(const XmlNode& node);
    TerrainOptions loadTerrainOptions(const XmlNode& node);
    void loadPrefabs(const XmlNode& node);
};

WorldLoaderImpl::WorldLoaderImpl(FileSystem& fileSystem, EntityFactory& entityFactory,
  ResourceManager& resourceManager)
  : m_fileSystem(fileSystem)
  , m_entityFactory(entityFactory)
  , m_resourceManager(resourceManager)
{
  auto worldFilePath = fs::path{"worlds"} / m_worldName / "world.xml";

  auto xmlData = m_fileSystem.readAppDataFile(worldFilePath);
  auto worldXml = parseXml(xmlData);

  loadWorldInfo(*worldXml);
  auto terrainOptions = loadTerrainOptions(*worldXml->child("terrain"));
  m_terrainBuilder = createTerrainBuilder(terrainOptions);
  loadPrefabs(*worldXml->child("prefabs"));
}

const WorldInfo& WorldLoaderImpl::worldInfo() const
{
  return m_worldInfo;
}

ResourceHandle WorldLoaderImpl::loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx)
{
  return m_resourceManager.loadResource([this, x, y, sliceIdx](ResourceId) {
    std::stringstream ss;
    ss << std::setw(3) << std::setfill('0') << x << y << sliceIdx << ".xml";

    auto fileData = m_fileSystem.readAppDataFile(fs::path{"worlds"} / m_worldName / ss.str());
    auto node = parseXml(fileData);

    // TODO

    return ManagedResource{
      .provider = weak_from_this(),
      .unloader = [](ResourceId) {}
    };
  });
}

TerrainOptions WorldLoaderImpl::loadTerrainOptions(const XmlNode& node)
{
  auto heightMap = node.attribute("height-map");
  int gridSizeX = std::stoi(node.attribute("grid-size-x"));
  int gridSizeY = std::stoi(node.attribute("grid-size-y"));
  float sizeX = std::stof(node.attribute("size-x"));
  float sizeY = std::stof(node.attribute("size-y"));

  // TODO
  return TerrainOptions{

  };
}

void WorldLoaderImpl::loadPrefabs(const XmlNode& node)
{

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

WorldLoaderPtr createWorldLoader(FileSystem& fileSystem, TerrainBuilder& terrainBuilder,
  EntityFactory& entityFactory, ResourceManager& resourceManager)
{
  return std::make_unique<WorldLoaderImpl>(fileSystem, terrainBuilder, entityFactory,
    resourceManager);
}

} // namespace lithic3d
