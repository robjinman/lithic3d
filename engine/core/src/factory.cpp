#include "lithic3d/factory.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/sys_collision.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/utils.hpp"

namespace lithic3d
{

using render::Material;
using render::MaterialFeatureSet;
namespace MaterialFeatures = render::MaterialFeatures;
using render::MaterialHandle;
using render::MeshFeatureSet;
namespace MeshFeatures = render::MeshFeatures;
using render::MeshHandle;
using render::BufferUsage;
using render::MeshPtr;

namespace
{

class FactoryImpl : public Factory
{
  public:
    FactoryImpl(Ecs& ecs, ModelLoader&  modelLoader, RenderResourceLoader& renderResourceLoader);

    // TODO: Use metres, not world units

    EntityId createShape(EntityId parentId, MeshPtr mesh, const Vec4f& colour) override;
    EntityId createShape(EntityId parentId, MeshPtr mesh, MaterialHandle material) override;
    MaterialHandle createMaterialAsync(const std::filesystem::path& texturePath,
      bool genMipmaps) override;
    EntityId createStaticCuboid(EntityId parentId, const Vec3f& sizeInMetres,
      MaterialHandle material, float restitution, float friction) override;
    EntityId createDynamicCuboid(EntityId parentId, const Vec3f& sizeInMetres,
      MaterialHandle material, float inverseMass, float restitution, float friction) override;

  private:
    Ecs& m_ecs;
    ModelLoader& m_modelLoader;
    RenderResourceLoader& m_renderResourceLoader;
    MeshHandle m_cubeMesh;

    void createCuboidCommonComponents(EntityId id, EntityId parentId, const Vec3f& size,
      MaterialHandle material);
    void createCubeMesh();
};

FactoryImpl::FactoryImpl(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader)
  : m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderResourceLoader(renderResourceLoader)
{
  createCubeMesh();
}

void FactoryImpl::createCubeMesh()
{
  auto mesh = render::cuboid({ 1.f, 1.f, 1.f }, { 1.f, 1.f });
  mesh->featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{ bitflag(MeshFeatures::CastsShadow) | bitflag(MeshFeatures::IsInstanced) } // TODO
  };
  mesh->maxInstances = 2000; // TODO

  m_cubeMesh = m_renderResourceLoader.loadMeshAsync(std::move(mesh)).wait();
}

// TODO: Move this
Aabb computeAabb(const render::Mesh& mesh)
{
  auto& posBuffer = getBuffer(mesh.attributeBuffers, BufferUsage::AttrPosition);
  auto positions = posBuffer.data.data<Vec3f>();

  const float maxf = std::numeric_limits<float>::max();
  const float minf = std::numeric_limits<float>::lowest();

  Aabb aabb{
    .min = { maxf, maxf, maxf },
    .max = { minf, minf, minf }
  };

  for (auto& pos : positions) {
    for (size_t i = 0; i < 3; ++i) {
      if (pos[i] < aabb.min[i]) {
        aabb.min[i] = pos[i];
      }
      if (pos[i] > aabb.max[i]) {
        aabb.max[i] = pos[i];
      }
    }
  }

  return aabb;
}

EntityId FactoryImpl::createShape(EntityId parentId, MeshPtr mesh, MaterialHandle material)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();

  auto id = m_ecs.idGen().getNewEntityId();
  m_ecs.componentStore().allocate<DSpatial, DModel>(id);

  DSpatial spatial{};
  spatial.parent = parentId;
  spatial.aabb = computeAabb(*mesh);

  sysSpatial.addEntity(id, spatial);

  auto model = std::make_unique<Model>();
  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .lods = { m_renderResourceLoader.loadMeshAsync(std::move(mesh)).wait() },
      .material = material,
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  auto render = std::make_unique<DModel>();
  render->model = m_modelLoader.loadModelAsync(std::move(model)).wait();

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

EntityId FactoryImpl::createShape(EntityId parentId, MeshPtr mesh, const Vec4f& colour)
{
  auto material = std::make_unique<Material>();
  material->colour = colour;

  auto materialHandle = m_renderResourceLoader.loadMaterialAsync(std::move(material));

  return createShape(parentId, std::move(mesh), materialHandle);
}

MaterialHandle FactoryImpl::createMaterialAsync(const std::filesystem::path& texturePath,
  bool genMipmaps)
{
  auto material = std::make_unique<Material>();
  material->featureSet = MaterialFeatureSet{
    .flags{
      bitflag(MaterialFeatures::HasTexture)
    }
  };
  material->textures = { m_renderResourceLoader.loadTextureAsync(texturePath, genMipmaps) };

  return m_renderResourceLoader.loadMaterialAsync(std::move(material));
}

void FactoryImpl::createCuboidCommonComponents(EntityId id, EntityId parentId,
  const Vec3f& sizeInMetres, MaterialHandle material)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();

  DSpatial spatial{};
  spatial.parent = parentId;
  spatial.aabb = {
    .min = metresToWorldUnits(Vec3f{ -0.5f, -0.5f, -0.5f }),
    .max = metresToWorldUnits(Vec3f{ 0.5f, 0.5f, 0.5f })
  };
  spatial.transform = scaleMatrix4x4(sizeInMetres);

  sysSpatial.addEntity(id, spatial);

  auto model = std::make_unique<Model>();
  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .lods = { m_cubeMesh },
      .material = material.wait(),
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  auto render = std::make_unique<DModel>();
  render->model = m_modelLoader.loadModelAsync(std::move(model)).wait();
  render->isInstanced = true; // TODO

  sysRender3d.addEntity(id, std::move(render));
}

EntityId FactoryImpl::createStaticCuboid(EntityId parentId, const Vec3f& sizeInMetres,
  MaterialHandle material, float restitution, float friction)
{
  auto id = m_ecs.idGen().getNewEntityId();
  m_ecs.componentStore().allocate<DSpatial, DModel, DStaticBox>(id);

  auto& sysCollision = m_ecs.system<SysCollision>();

  createCuboidCommonComponents(id, parentId, sizeInMetres, material);

  DStaticBox collision{
    .restitution = restitution,
    .friction = friction,
    .boundingBox{
      .min = metresToWorldUnits(Vec3f{ -0.5f, -0.5f, -0.5f }),
      .max = metresToWorldUnits(Vec3f{ 0.5f, 0.5f, 0.5f }),
      .transform = identityMatrix<4>()
    }
  };
  sysCollision.addEntity(id, collision);

  return id;
}

EntityId FactoryImpl::createDynamicCuboid(EntityId parentId, const Vec3f& sizeInMetres,
  MaterialHandle material, float inverseMass, float restitution, float friction)
{
  auto id = m_ecs.idGen().getNewEntityId();
  m_ecs.componentStore().allocate<DSpatial, DModel, DDynamicBox>(id);

  auto& sysCollision = m_ecs.system<SysCollision>();

  createCuboidCommonComponents(id, parentId, sizeInMetres, material);

  DDynamicBox collision{
    .inverseMass = inverseMass,
    .restitution = restitution,
    .friction = friction,
    .centreOfMass = { 0.f, 0.f, 0.f },
    .boundingBox{
      .min = metresToWorldUnits(Vec3f{ -0.5f, -0.5f, -0.5f }),
      .max = metresToWorldUnits(Vec3f{ 0.5f, 0.5f, 0.5f }),
      .transform = identityMatrix<4>()
    }
  };
  sysCollision.addEntity(id, collision);

  return id;
}

} // namespace

FactoryPtr createFactory(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader)
{
  return std::make_unique<FactoryImpl>(ecs, modelLoader, renderResourceLoader);
}

} // namespace lithic3d
