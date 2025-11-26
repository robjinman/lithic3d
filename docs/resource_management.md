Resource Management
===================

Almost all resources in Lithic3D are managed resources: Sounds, Meshes, Textures, Models, Entities, etc. Some of these resources are made up of other resources and some resources are shared among multiple resources.

The ResourceManager class keeps track of which resources depend on other resources and uses reference counting to ensure that resources are only unloaded when no longer needed.

Any class that provides managed resources, whether that's a factory or a subsystem, must create its resources asynchonously, returning an `std::future<ResourceId>`, for example:

```
    std::future<ResourceId> createModelAsync(const std::string& filePath, ResourceId materialId);
```

The implementation of this function should call ResourceManager::loadResource providing a function that creates the resource and returns Resource object describing the resource. This function will be executed on the resource manager's thread.

```
    std::future<ResourceId> createModelAsync(const std::string& filePath, ResourceId materialId)
    {
      return m_resourceManager.loadResource([this, filePath, material](ResourceId id) {
        // Construct resource here, including any sub-resources

        auto& material = m_materialFactory.getMaterial(materialId);
        // Do something with material

        // Construct a sub-resource
        auto subresourceId = m_factory.createThingAsync("foo", 123).get();

        return Resource{
          .unload = [this](ResourceId id) { deleteModel(id); },
          .dependencies = { materialId, subresourceId }
        };
      });
    }
```

The resource manager when it needs to unload the resource will call the unloader function provided by the Resource instance. This function may be called at any time from the resource manager's own thread and so must be thread-safe.

See the unit tests for examples.
