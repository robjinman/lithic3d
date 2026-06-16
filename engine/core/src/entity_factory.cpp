#include "lithic3d/entity_factory.hpp"
#include "lithic3d/game_data_paths.hpp"
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

struct Prefab
{
  std::vector<ComponentDataPtr> components;
};

class EntityFactoryImpl : public EntityFactory
{
  public:
    EntityFactoryImpl(Ecs& ecs, ResourceManager& resourceManager, const GameDataPaths& paths,
      Logger& logger);

    ResourceHandle loadPrefabAsync(const std::string& name) override;
    bool hasEntityType(const std::string& type) const override;
    EntityId constructEntity(EntityId parentId, const std::string& type,
      const Mat4x4f& transform) const override;
    EntityId constructEntity(EntityId parentId, const XmlNode& xmlEntity,
      EntityMask& changedFromPrefab, std::vector<XmlNodePtr>& unused) const override;
    EntityId constructGhostEntity(EntityId parentId, const std::string& type,
      const Mat4x4f& transform, const Vec4f& colour) override;
    bool hasPrefab(const std::string& name) const override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    ResourceManager& m_resourceManager;
    const GameDataPaths& m_paths;
    std::unordered_map<std::string, SystemId> m_systemNames;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Prefab> m_prefabs;
    std::vector<ComponentSpec> getComponentSpecs(const Prefab& prefab) const;
};

EntityFactoryImpl::EntityFactoryImpl(Ecs& ecs, ResourceManager& resourceManager,
  const GameDataPaths& paths, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_resourceManager(resourceManager)
  , m_paths(paths)
{
  for (SystemId i = 0; i < m_ecs.numSystems(); ++i) {
    m_systemNames[m_ecs.getSystem(i).name()] = i;
  }
}

ResourceHandle EntityFactoryImpl::loadPrefabAsync(const std::string& name)
{
  return m_resourceManager.loadResource([this, name](ResourceId) {
    auto data = m_paths.prefabsDir->readFile(STR(name << ".xml"));
    auto entityXml = parseXml(data);

    ASSERT(entityXml->name() == "entity", "Expected <entity> element");
    ASSERT(entityXml->attribute("type") == name, "Expected entity of type '" << name << "'");

    Prefab prefab;
    prefab.components.resize(m_ecs.numSystems());
    for (size_t systemId = 0; systemId < prefab.components.size(); ++systemId) {
      auto& system = m_ecs.getSystem(systemId);
      auto i = entityXml->child(system.name());
      if (i != entityXml->end()) {
        prefab.components[systemId] = system.constructComponentData(*i);
      }
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

std::vector<ComponentSpec> EntityFactoryImpl::getComponentSpecs(const Prefab& prefab) const
{
  std::vector<ComponentSpec> specs;

  for (size_t systemId = 0; systemId < prefab.components.size(); ++systemId) {
    if (prefab.components[systemId] != nullptr) {
      auto& system = m_ecs.getSystem(systemId);
      system.extractComponentSpecs(*prefab.components[systemId], specs);
    }
  }

  return specs;
}

bool EntityFactoryImpl::hasEntityType(const std::string& type) const
{
  return m_prefabs.contains(type);
}

void setParentIdIfSpatialComponent(ComponentData& c, EntityId parentId)
{
  if (c.typeId() == typeid(DSpatial).hash_code()) {
    auto& spatial = dynamic_cast<ComponentDataWrapper<DSpatial>&>(c).data();
    spatial.parent = parentId;
  }
}

EntityId EntityFactoryImpl::constructEntity(EntityId parentId, const XmlNode& xmlEntity,
  EntityMask& changedFromPrefab, std::vector<XmlNodePtr>& unused) const
{
  std::scoped_lock lock{m_mutex};

  auto type = xmlEntity.attribute("type");
  auto& prefab = m_prefabs.at(type);

  EntityId id = NULL_ENTITY_ID;

  std::string entityName = xmlEntity.attribute("id");
  if (!entityName.empty()) {
    id = m_ecs.idGen().getNewEntityId(entityName);
  }
  else {
    id = m_ecs.idGen().getNewEntityId();
  }

  m_logger.debug(STR("Constructing entity " << id << " of type " << type));

  auto specs = getComponentSpecs(prefab);
  m_ecs.componentStore().allocateEntity(id, specs);

  // First pass looking for unknown system components
  for (auto& xmlSystem : xmlEntity) {
    auto i = m_systemNames.find(xmlSystem.name());
    if (i == m_systemNames.end()) {
      unused.push_back(xmlSystem.clone());
    }
  }

  for (size_t systemId = 0; systemId < prefab.components.size(); ++systemId) {
    if (prefab.components[systemId] != nullptr) {
      auto& system = m_ecs.getSystem(systemId);
      auto i = xmlEntity.child(system.name());
      if (i != xmlEntity.end()) {
        auto c = system.constructComponentDataWithModifications(*prefab.components[systemId], *i);
        setParentIdIfSpatialComponent(*c, parentId);
        system.addEntity(id, *c);

        if (systemId < Systems::NUMBER_OF_SYSTEMS) {
          changedFromPrefab[systemId] = c->mask();
        }
      }
      else {
        system.addEntity(id, *prefab.components[systemId]);

        if (systemId < Systems::NUMBER_OF_SYSTEMS) {
          changedFromPrefab[systemId] = 0;
        }
      }
    }
  }

  return id;
}

EntityId EntityFactoryImpl::constructEntity(EntityId parentId, const std::string& type,
  const Mat4x4f& transform) const
{
  std::scoped_lock lock{m_mutex};

  auto& prefab = m_prefabs.at(type);

  auto id = m_ecs.idGen().getNewEntityId();

  m_logger.debug(STR("Constructing entity " << id << " of type " << type));

  auto specs = getComponentSpecs(prefab);
  m_ecs.componentStore().allocateEntity(id, specs);

  for (size_t systemId = 0; systemId < prefab.components.size(); ++systemId) {
    if (prefab.components[systemId] != nullptr) {
      auto& system = m_ecs.getSystem(systemId);

      if (systemId == Systems::Spatial) {
        auto& data = *prefab.components[systemId];
        assert(data.typeId() == typeid(DSpatial).hash_code());
        auto& spatialWrapper = dynamic_cast<ComponentDataWrapper<DSpatial>&>(data);
        auto spatial = spatialWrapper.data(); // Copy
        spatial.parent = parentId;
        spatial.transform = transform;
        m_ecs.system<SysSpatial>().addEntity(id, spatial);
      }
      else {
        system.addEntity(id, *prefab.components[systemId]);
      }
    }
  }

  return id;
}

EntityId EntityFactoryImpl::constructGhostEntity(EntityId parentId, const std::string& type,
  const Mat4x4f& transform, const Vec4f& colour)
{
  std::scoped_lock lock{m_mutex};

  auto& prefab = m_prefabs.at(type);

  auto id = m_ecs.idGen().getNewEntityId();

  m_logger.debug(STR("Constructing ghost entity " << id << " of type " << type));

  auto specs = getComponentSpecs(prefab);
  m_ecs.componentStore().allocateEntity(id, specs);

  if (prefab.components[Systems::Spatial] != nullptr) {
    auto& data = *prefab.components[Systems::Spatial];
    assert(data.typeId() == typeid(DSpatial).hash_code());
    auto& spatialWrapper = dynamic_cast<ComponentDataWrapper<DSpatial>&>(data);
    auto spatial = spatialWrapper.data(); // Copy
    spatial.parent = parentId;
    spatial.transform = transform;
    m_ecs.system<SysSpatial>().addEntity(id, spatial);
  }

  if (prefab.components[Systems::Render3d] != nullptr) {
    auto& data = *prefab.components[Systems::Render3d];
    assert(data.typeId() == typeid(DModel).hash_code());
    auto& renderWrapper = dynamic_cast<ComponentDataWrapper<DModel>&>(data);
    DModelPtr render = std::make_unique<DModel>(renderWrapper.data());

    render->colour = colour;
    m_ecs.system<SysRender3d>().addEntity(id, std::move(render));
  }

  return id;
}

bool EntityFactoryImpl::hasPrefab(const std::string& name) const
{
  std::scoped_lock lock{m_mutex};

  return m_prefabs.contains(name);
}

} // namespace

EntityFactoryPtr createEntityFactory(Ecs& ecs, ResourceManager& resourceManager,
  const GameDataPaths& paths, Logger& logger)
{
  return std::make_unique<EntityFactoryImpl>(ecs, resourceManager, paths, logger);
}

} // namespace lithic3d
