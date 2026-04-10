Resource Management
===================

Almost all resources in Lithic3D are managed resources: Sounds, Meshes, Textures, Models, Prefabs, etc. Some of these resources are made up of other resources and some resources are shared among multiple resources.

Entities are NOT resources as they need to be created on the main (simulation) thread synchronously.

The ResourceManager class keeps track of which resources depend on other resources and uses reference counting to ensure that resources are only unloaded when no longer needed.

Any class that provides managed resources, whether that's a factory or a subsystem, should create its resources asynchonously, returning a ResourceHandle, for example:

```
    ResourceHandle createModelAsync(const std::string& filePath, ResourceHandle material);
```

The implementation of this function should call ResourceManager::loadResource, providing a ResourceLoader function that creates the resource and returns a ManagedResource object describing the resource. This loader function will be executed on the resource manager's thread.

The class should call waitAll() on the ResourceManager in its destructor.

```
    class ModelFactory
    {
      public:
        ResourceHandle createModelAsync(const std::string& filePath, ResourceHandle hMaterial)
        {
          return m_resourceManager.loadResource([this, filePath, material](ResourceId id) {
            // We're on the resource manager thread (make thread-safe!)
            // Construct resource here, including any sub-resources

            hMaterial.wait(); // To be sure it exists
            auto& material = m_materialFactory.getMaterial(hMaterial.id());

            // Do something with material?

            // Construct a sub-resource
            auto hMesh = m_factory.createMeshAsync(filePath);

            // Store the resource somewhere
            m_models.insert(id, std::make_unique<Model>(hSubresource, hMaterial, hMesh));

            return ManagedResource{
              .unloader = [this](ResourceId id) { deleteModel(id); }
            };
          });
        }

        void deleteModel(ResourceId id)
        {
          // We're on the resource manager thread (make thread-safe!)
          // Delete the model. Sub-resources will be automatically unloaded if their reference
          // counts hit zero.

          m_models.erase(id);
        }

        ~ModelFactory()
        {
          m_resourceManager.waitAll();
        }
    };
```

See the unit tests for examples.
