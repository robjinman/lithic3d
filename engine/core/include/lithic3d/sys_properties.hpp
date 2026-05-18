#pragma once

#include "systems.hpp"
#include <unordered_map>

namespace lithic3d
{

struct DProperties
{
  using RequiredComponents = type_list<>;

  std::unordered_map<std::string, int> ints;
  std::unordered_map<std::string, float> floats;
  std::unordered_map<std::string, bool> bools;
  std::unordered_map<std::string, std::string> strings;
};

class SysProperties : public System
{
  public:
    virtual void addEntity(EntityId id, const DProperties& data) = 0;

    virtual void setIntProperty(EntityId entityId, const std::string& name, int value) = 0;
    virtual void setFloatProperty(EntityId entityId, const std::string& name, float value) = 0;
    virtual void setBoolProperty(EntityId entityId, const std::string& name, bool value) = 0;
    virtual void setStringProperty(EntityId entityId, const std::string& name,
      const std::string& value) = 0;

    virtual int getIntProperty(EntityId entityId, const std::string& name) const = 0;
    virtual float getFloatProperty(EntityId entityId, const std::string& name) const = 0;
    virtual bool getBoolProperty(EntityId entityId, const std::string& name) const = 0;
    virtual const std::string& getStringProperty(EntityId entityId,
      const std::string& name) const = 0;

    virtual ~SysProperties() = default;

    static const SystemId id = Systems::Properties;
};

using SysPropertiesPtr = std::unique_ptr<SysProperties>;

SysPropertiesPtr createSysProperties();

} // namespace lithic3d
