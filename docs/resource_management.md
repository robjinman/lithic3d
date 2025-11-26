Resource Management
===================

Almost all resources in Lithic3D are managed resources: Sounds, Meshes, Textures, Models, Entities, etc. Some of these resources are made up of other resources and some resources are shared among multiple resources.

The ResourceManager class keeps track of which resources depend on other resources and uses reference counting to ensure that resources are only unloaded when no longer needed.

Any class that provides managed resources, whether that's a factory or a subsystem, must inherit from ResourceProvider and override its createResource method. Within the createResource method the derived class should register the resource with the ResourceManager via the addResource method providing a Resource instance describing the resource.

The resource manager when it needs to load or unload the resource will call the loader or unloader functions provided by the Resource instance. These functions may be called at any time from the resource manager's own thread and so must be thread-safe.

See the unit tests for examples.
