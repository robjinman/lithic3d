#include "lithic3d/scene.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/entity_factory.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/terrain_system.hpp"

namespace fs = std::filesystem;

namespace lithic3d
{

class SceneBuilder
{
  public:
    SceneBuilder(FileSystem& fileSystem, TerrainSystem& terrainSystem, EntityFactory& entityFactory,
      ResourceManager& resourceManager);

    ScenePtr loadScene(const std::string& name);

  private:
    FileSystem& m_fileSystem;
    TerrainSystem& m_terrainSystem;
    EntityFactory& m_entityFactory;
    ResourceManager& m_resourceManager;

    void loadResources(const XmlNode& node);
    void loadTerrain(const XmlNode& node);
    void loadPrefabs(const XmlNode& node);
};

SceneBuilder::SceneBuilder(FileSystem& fileSystem, TerrainSystem& terrainSystem,
  EntityFactory& entityFactory, ResourceManager& resourceManager)
  : m_fileSystem(fileSystem)
  , m_terrainSystem(terrainSystem)
  , m_entityFactory(entityFactory)
  , m_resourceManager(resourceManager)
{
}

void SceneBuilder::loadResources(const XmlNode& node)
{
  auto texturesXml = node.child("textures");
  if (texturesXml != node.end()) {
    // TODO: Load textures
  }

  auto materialsXml = node.child("materials");
  if (materialsXml != node.end()) {
    // TODO: Load materials
  }

  auto modelsXml = node.child("models");
  if (modelsXml != node.end()) {
    // TODO: Load models
  }
}

void SceneBuilder::loadTerrain(const XmlNode& node)
{
  auto heightMap = node.attribute("height-map");
  int gridSizeX = std::stoi(node.attribute("grid-size-x"));
  int gridSizeZ = std::stoi(node.attribute("grid-size-z"));
  float sizeX = std::stof(node.attribute("size-x"));
  float sizeY = std::stof(node.attribute("size-y"));

  m_terrainSystem.loadHeightMap(fs::path{"textures"} / heightMap);
}

void SceneBuilder::loadPrefabs(const XmlNode& node)
{

}

ScenePtr SceneBuilder::loadScene(const std::string& name)
{
  auto scene = std::make_unique<Scene>();

  auto sceneFilePath = fs::path{"scenes"} / name;

  auto xmlData = m_fileSystem.readAppDataFile(sceneFilePath);
  auto sceneXml = parseXml(xmlData);

  auto resourcesXml = sceneXml->child("resources");
  if (resourcesXml != sceneXml->end()) {
    loadResources(*resourcesXml);
  }

  auto prefabsXml = sceneXml->child("prefabs");
  if (prefabsXml != sceneXml->end()) {
    loadPrefabs(*prefabsXml);
  }

  auto terrainXml = sceneXml->child("terrain");
  if (terrainXml != sceneXml->end()) {
    loadTerrain(*terrainXml);
  }

  return scene;
}

ScenePtr loadScene(const std::string& name, FileSystem& fileSystem, TerrainSystem& terrainSystem,
  EntityFactory& entityFactory, ResourceManager& resourceManager)
{
  SceneBuilder builder{fileSystem, terrainSystem, entityFactory, resourceManager};
  return builder.loadScene(name);
}

} // namespace lithic3d
