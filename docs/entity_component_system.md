Conventions
-----------

Systems inherit System and are prefixed with Sys, e.g. SysRender. They should define a static data member of type SystemId that is unique to the class.

Components stored in the component store are prefixed with C and should be plain-old-data. Components should define a static TypeId data member of type ComponentType unique to the class.

The component store is used to facilitate fast iteration over components within a system's update() method. Systems don't have to use the component store at all.

Entity data passed into the system (and possibly used to populate component store components) are prefixed with D, e.g. DSpatial. These classes should define an inner type called RequiredComponents of type type_list<...> in which required components are listed.

Systems should provide helper functions for writing to components.
