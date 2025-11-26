#include "lithic3d/world.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/entity_factory.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/terrain_builder.hpp"

namespace fs = std::filesystem;

namespace lithic3d
{

class WorldLoader
{
  public:
    WorldLoader(FileSystem& fileSystem, TerrainBuilder& terrainBuilder,
      EntityFactory& entityFactory, ResourceManager& resourceManager);

    WorldPtr createWorld(const std::string& name);

  private:
    FileSystem& m_fileSystem;
    TerrainBuilder& m_terrainBuilder;
    EntityFactory& m_entityFactory;
    ResourceManager& m_resourceManager;

    void configureTerrainBuilder(const XmlNode& node);
    void loadPrefabs(const XmlNode& node);
};

WorldLoader::WorldLoader(FileSystem& fileSystem, TerrainBuilder& terrainBuilder,
  EntityFactory& entityFactory, ResourceManager& resourceManager)
  : m_fileSystem(fileSystem)
  , m_terrainBuilder(terrainBuilder)
  , m_entityFactory(entityFactory)
  , m_resourceManager(resourceManager)
{
}

void WorldLoader::configureTerrainBuilder(const XmlNode& node)
{
  auto heightMap = node.attribute("height-map");
  int gridSizeX = std::stoi(node.attribute("grid-size-x"));
  int gridSizeY = std::stoi(node.attribute("grid-size-y"));
  float sizeX = std::stof(node.attribute("size-x"));
  float sizeY = std::stof(node.attribute("size-y"));

  // TODO
}

void WorldLoader::loadPrefabs(const XmlNode& node)
{

}

WorldPtr WorldLoader::createWorld(const std::string& name)
{
  auto world = std::make_unique<World>();

  auto worldFilePath = fs::path{"worlds"} / name;

  auto xmlData = m_fileSystem.readAppDataFile(worldFilePath);
  auto worldXml = parseXml(xmlData);

  auto gridSizeX = std::stoi(worldXml->attribute("grid-size-x"));
  auto gridSizeY = std::stoi(worldXml->attribute("grid-size-y"));
  auto cellSizeX = std::stof(worldXml->attribute("cell-size-x"));
  auto cellSizeY = std::stof(worldXml->attribute("cell-size-y"));
  auto numLayers = std::stoi(worldXml->attribute("layers"));

  world->grid.sizeCells = { gridSizeX, gridSizeY };
  world->grid.sizeMetres = { gridSizeX * cellSizeX, gridSizeY * cellSizeY };

  auto prefabsXml = worldXml->child("prefabs");
  if (prefabsXml != worldXml->end()) {
    loadPrefabs(*prefabsXml);
  }

  auto terrainXml = worldXml->child("terrain");
  if (terrainXml != worldXml->end()) {
    configureTerrainBuilder(*terrainXml);
  }

  return world;
}

WorldPtr createWorld(const std::string& name, FileSystem& fileSystem,
  TerrainBuilder& terrainBuilder, EntityFactory& entityFactory, ResourceManager& resourceManager)
{
  WorldLoader loader{fileSystem, terrainBuilder, entityFactory, resourceManager};
  return loader.createWorld(name);
}

} // namespace lithic3d
