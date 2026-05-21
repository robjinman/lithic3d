#include "lithic3d/sys_properties.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/utils.hpp"

namespace lithic3d
{
namespace
{

class SysPropertiesImpl : public SysProperties
{
  public:
    SysPropertiesImpl() {}

    const std::string& name() const override;
    void extractComponentSpecs(const ComponentData& data,
      std::vector<ComponentSpec>& specs) const override;
    ComponentDataPtr constructComponentData(const XmlNode& data) const override;
    ComponentDataPtr constructComponentDataWithModifications(const ComponentData& base,
      const XmlNode& changes) const override;
    XmlNodePtr componentToXml(EntityId entityId) const override;
    void addEntity(EntityId id, const ComponentData& data) override;
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick, const InputState&) override {}
    void processEvent(const Event&) override {}

    void addEntity(EntityId id, const DProperties& properties) override;

    void setIntProperty(EntityId entityId, const std::string& name, int value) override;
    void setFloatProperty(EntityId entityId, const std::string& name, float value) override;
    void setBoolProperty(EntityId entityId, const std::string& name, bool value) override;
    void setStringProperty(EntityId entityId, const std::string& name,
      const std::string& value) override;

    int getIntProperty(EntityId entityId, const std::string& name) const override;
    float getFloatProperty(EntityId entityId, const std::string& name) const override;
    bool getBoolProperty(EntityId entityId, const std::string& name) const override;
    const std::string& getStringProperty(EntityId entityId, const std::string& name) const override;

  private:
    std::unordered_map<EntityId, DProperties> m_components;

    ComponentDataPtr constructDProperties(const XmlNode& data) const;
};

const std::string& SysPropertiesImpl::name() const
{
  static const std::string name = "properties";
  return name;
}

void SysPropertiesImpl::extractComponentSpecs(const ComponentData& data,
  std::vector<ComponentSpec>& specs) const
{
  extractSpecs<DProperties>(data, specs);
  // ...
}

ComponentDataPtr SysPropertiesImpl::constructDProperties(const XmlNode& xmlProperties) const
{
  DProperties properties;

  for (auto& node : xmlProperties) {
    auto value = trimWhitespace(node.value());
    if (node.name() == "string") {
      properties.strings[node.attribute("name")] = value;
    }
    else if (node.name() == "int") {
      properties.ints[node.attribute("name")] = std::stoi(value);
    }
    else if (node.name() == "float") {
      properties.floats[node.attribute("name")] = std::stof(value);
    }
    else if (node.name() == "bool") {
      properties.bools[node.attribute("name")] = value == "true";
    }
  }

  return std::make_unique<ComponentDataWrapper<DProperties>>(properties);
}

ComponentDataPtr SysPropertiesImpl::constructComponentData(const XmlNode& data) const
{
  if (data.name() == "properties") {
    return constructDProperties(data);
  }
  // ...
  else {
    EXCEPTION("Bad component data type for properties system");
  }
}

ComponentDataPtr SysPropertiesImpl::constructComponentDataWithModifications(
  const ComponentData& base, const XmlNode& xmlSysProperties) const
{
  auto& xmlComp = *xmlSysProperties.begin();
  if (xmlComp.name() == "properties") {
    assert(base.typeId() == typeid(DProperties).hash_code());
    auto& baseProps = dynamic_cast<const ComponentDataWrapper<DProperties>&>(base).data();

    auto newPropsData = constructDProperties(xmlComp);
    auto& newProps = dynamic_cast<ComponentDataWrapper<DProperties>&>(*newPropsData).data();

    newProps.bools.insert(baseProps.bools.begin(), baseProps.bools.end());
    newProps.ints.insert(baseProps.ints.begin(), baseProps.ints.end());
    newProps.floats.insert(baseProps.floats.begin(), baseProps.floats.end());
    newProps.strings.insert(baseProps.strings.begin(), baseProps.strings.end());

    return newPropsData;
  }
  // ...
  else {
    EXCEPTION("Bad component data type for properties system");
  }
}

template<typename T>
void writeToXml(XmlNode& parent, const std::string& typeName,
  const std::unordered_map<std::string, T>& map)
{
  for (auto& entry : map) {
    auto node = createXmlNode(typeName);
    node->setAttribute("name", entry.first);
    if constexpr (std::is_same<T, std::string>::value) {
      node->setValue(entry.second);
    }
    else {
      node->setValue(std::to_string(entry.second));
    }
    parent.addChild(std::move(node));
  }
}

XmlNodePtr SysPropertiesImpl::componentToXml(EntityId entityId) const
{
  auto i = m_components.find(entityId);
  if (i == m_components.end()) {
    return nullptr;
  }
  auto& comp = i->second;

  auto xmlSysProperties = createXmlNode("properties");
  auto xmlProperties = createXmlNode("properties");

  writeToXml(*xmlProperties, "string", comp.strings);
  writeToXml(*xmlProperties, "int", comp.ints);
  writeToXml(*xmlProperties, "float", comp.floats);
  writeToXml(*xmlProperties, "bool", comp.bools);

  xmlSysProperties->addChild(std::move(xmlProperties));

  return xmlSysProperties;
}

void SysPropertiesImpl::addEntity(EntityId id, const ComponentData& data)
{
  if (data.typeId() == typeid(DProperties).hash_code()) {
    auto& properties = dynamic_cast<const ComponentDataWrapper<DProperties>&>(data).data();
    addEntity(id, properties);
  }
  else {
    EXCEPTION("Bad component data type for properties system");
  }
}

void SysPropertiesImpl::removeEntity(EntityId entityId)
{
  m_components.erase(entityId);
}

bool SysPropertiesImpl::hasEntity(EntityId entityId) const
{
  return m_components.contains(entityId);
}

void SysPropertiesImpl::addEntity(EntityId id, const DProperties& data)
{
  m_components[id] = data;
}

void SysPropertiesImpl::setIntProperty(EntityId entityId, const std::string& name, int value)
{
  m_components[entityId].ints[name] = value;
}

void SysPropertiesImpl::setFloatProperty(EntityId entityId, const std::string& name, float value)
{
  m_components[entityId].floats[name] = value;
}

void SysPropertiesImpl::setBoolProperty(EntityId entityId, const std::string& name, bool value)
{
  m_components[entityId].bools[name] = value;
}

void SysPropertiesImpl::setStringProperty(EntityId entityId, const std::string& name,
  const std::string& value)
{
  m_components[entityId].strings[name] = value;
}

int SysPropertiesImpl::getIntProperty(EntityId entityId, const std::string& name) const
{
  auto iProps = m_components.find(entityId);
  ASSERT(iProps != m_components.end(), "No properties component for entity " << entityId);

  auto& map = iProps->second.ints;
  auto iMap = map.find(name);
  ASSERT(iMap != map.end(), "Entity " << entityId << " has no int property '" << name << "'");

  return iMap->second;
}

float SysPropertiesImpl::getFloatProperty(EntityId entityId, const std::string& name) const
{
  auto iProps = m_components.find(entityId);
  ASSERT(iProps != m_components.end(), "No properties component for entity " << entityId);

  auto& map = iProps->second.floats;
  auto iMap = map.find(name);
  ASSERT(iMap != map.end(), "Entity " << entityId << " has no float property '" << name << "'");

  return iMap->second;
}

bool SysPropertiesImpl::getBoolProperty(EntityId entityId, const std::string& name) const
{
  auto iProps = m_components.find(entityId);
  ASSERT(iProps != m_components.end(), "No properties component for entity " << entityId);

  auto& map = iProps->second.bools;
  auto iMap = map.find(name);
  ASSERT(iMap != map.end(), "Entity " << entityId << " has no bool property '" << name << "'");

  return iMap->second;
}

const std::string& SysPropertiesImpl::getStringProperty(EntityId entityId,
  const std::string& name) const
{
  auto iProps = m_components.find(entityId);
  ASSERT(iProps != m_components.end(), "No properties component for entity " << entityId);

  auto& map = iProps->second.strings;
  auto iMap = map.find(name);
  ASSERT(iMap != map.end(), "Entity " << entityId << " has no string property '" << name << "'");

  return iMap->second;
}

} // namespace

SysPropertiesPtr createSysProperties()
{
  return std::make_unique<SysPropertiesImpl>();
}

} // namespace lithic3d
