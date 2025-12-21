#include "lithic3d/entity_factory.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/model_loader.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/logger.hpp"

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

struct Prefab
{
  std::optional<DSpatial> spatial;
  std::optional<DModelPtr> model;
};

class EntityFactoryImpl : public EntityFactory
{
  public:
    EntityFactoryImpl(Ecs& ecs, ModelLoader& modelLoader,
      RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
      const FileSystem& fileSystem, Logger& logger);

    ResourceHandle loadPrefabAsync(const std::string& name) override;
    EntityId constructEntity(const std::string& type, const Mat4x4f& transform) const override;
    bool hasPrefab(const std::string& name) const override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    ModelLoader& m_modelLoader;
    RenderResourceLoader& m_renderResourceLoader;
    ResourceManager& m_resourceManager;
    const FileSystem& m_fileSystem;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Prefab> m_prefabs;

    DSpatial constructDSpatial(const XmlNode& spatialXml) const;
    DModelPtr constructDModel(const XmlNode& modelXml) const;
    Aabb constructAabb(const XmlNode& aabbXml) const;
};

EntityFactoryImpl::EntityFactoryImpl(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderResourceLoader(renderResourceLoader)
  , m_resourceManager(resourceManager)
  , m_fileSystem(fileSystem)
{}

Aabb EntityFactoryImpl::constructAabb(const XmlNode& aabbXml) const
{
  return Aabb{
    .min = constructVec3f(*aabbXml.child("min")),
    .max = constructVec3f(*aabbXml.child("max"))
  };
}

DSpatial EntityFactoryImpl::constructDSpatial(const XmlNode& spatialXml) const
{
  DSpatial spatial;
  spatial.aabb = constructAabb(*spatialXml.child("aabb"));
  spatial.enabled = true;
  spatial.parent = m_ecs.system<SysSpatial>().root(); // TODO

  // TODO: transform?

  return spatial;
}

DModelPtr EntityFactoryImpl::constructDModel(const XmlNode& modelXml) const
{
  const fs::path modelsPath = "models";

  auto model = std::make_unique<DModel>();
  model->isInstanced = modelXml.attribute("is-instanced") == "true";

  auto modelFile = modelXml.attribute("file");
  model->model = m_modelLoader.loadModelAsync(modelsPath / modelFile);

  return model;
}

ResourceHandle EntityFactoryImpl::loadPrefabAsync(const std::string& name)
{
  return m_resourceManager.loadResource([this, name](ResourceId id) {
    const fs::path prefabsPath = "prefabs";
    auto data = m_fileSystem.readAppDataFile(prefabsPath / STR(name << ".xml"));
    auto entityXml = parseXml(data);

    ASSERT(entityXml->name() == "entity", "Expected <entity> element");
    ASSERT(entityXml->attribute("name") == name, "Expected entity of type '" << name << "'");

    Prefab prefab;

    auto i = entityXml->child("spatial");
    if (i != entityXml->end()) {
      prefab.spatial = constructDSpatial(*i);
    }

    i = entityXml->child("model");
    if (i != entityXml->end()) {
      prefab.model = constructDModel(*i);
    }

    {
      std::scoped_lock lock{m_mutex};
      m_prefabs.insert({ name, std::move(prefab) });
    }

    return ManagedResource{
      .unloader = [this, name](ResourceId) {
        std::scoped_lock lock{m_mutex};
        m_prefabs.erase(name);
      }
    };
  });
}

std::vector<ComponentSpec> getComponentSpecs(const Prefab& prefab)
{
  std::vector<ComponentSpec> specs;

  if (prefab.spatial.has_value()) {
    extractSpecs<DSpatial>(specs);
  }

  if (prefab.model.has_value()) {
  }

  return specs;
}

EntityId EntityFactoryImpl::constructEntity(const std::string& type, const Mat4x4f& transform) const
{
  std::scoped_lock lock{m_mutex};

  auto& prefab = m_prefabs.at(type);

  auto id = m_ecs.idGen().getNewEntityId();

  m_logger.debug(STR("Constructing entity " << id << " of type " << type));

  auto specs = getComponentSpecs(prefab);
  m_ecs.componentStore().allocateEntity(id, specs);

  if (prefab.spatial.has_value()) {
    auto spatial = prefab.spatial.value();
    spatial.transform = transform;
    m_ecs.system<SysSpatial>().addEntity(id, spatial);
  }

  if (prefab.model.has_value()) {
    auto model = std::make_unique<DModel>(*prefab.model.value());
    m_ecs.system<SysRender3d>().addEntity(id, std::move(model));
  }

  // ...

  return id;
}

bool EntityFactoryImpl::hasPrefab(const std::string& name) const
{
  std::scoped_lock lock{m_mutex};

  return m_prefabs.contains(name);
}

} // namespace

EntityFactoryPtr createEntityFactory(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
  const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<EntityFactoryImpl>(ecs, modelLoader, renderResourceLoader,
    resourceManager, fileSystem, logger);
}

} // namespace lithic3d
