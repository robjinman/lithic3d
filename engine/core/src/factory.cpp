#include "lithic3d/factory.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/render_resource_loader.hpp"

namespace lithic3d
{

using render::Material;
using render::MaterialFeatureSet;
namespace MaterialFeatures = render::MaterialFeatures;
using render::MaterialHandle;
using render::MeshFeatureSet;
namespace MeshFeatures = render::MeshFeatures;
using render::MeshHandle;
using render::bitflag;
using render::BufferUsage;

namespace
{

class FactoryImpl : public Factory
{
  public:
    FactoryImpl(Ecs& ecs, ModelLoader&  modelLoader, RenderResourceLoader& renderResourceLoader);

    MaterialHandle createMaterial(const std::filesystem::path& texturePath) override;
    EntityId createCuboid(const Vec3f& size, MaterialHandle material,
      const Vec2f& textureSize) override;

  private:
    Ecs& m_ecs;
    ModelLoader& m_modelLoader;
    RenderResourceLoader& m_renderResourceLoader;
};

FactoryImpl::FactoryImpl(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader)
  : m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderResourceLoader(renderResourceLoader)
{}

MaterialHandle FactoryImpl::createMaterial(const std::filesystem::path& texturePath)
{
  auto material = std::make_unique<Material>();
  material->featureSet = MaterialFeatureSet{
    .flags{
      bitflag(MaterialFeatures::HasTexture)
    }
  };
  material->textures = { m_renderResourceLoader.loadTextureAsync(texturePath) };

  return m_renderResourceLoader.loadMaterialAsync(std::move(material));
}

EntityId FactoryImpl::createCuboid(const Vec3f& size, MaterialHandle material,
  const Vec2f& textureSize)
{
  auto id = m_ecs.idGen().getNewEntityId();
  m_ecs.componentStore().allocate<DSpatial, DModel>(id);

  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();
  spatial.aabb = {
    .min = -size * 0.5f,
    .max = size * 0.5f
  };

  sysSpatial.addEntity(id, spatial);

  auto mesh = render::cuboid(size, textureSize);
  mesh->featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };

  auto model = std::make_unique<Model>();
  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .mesh = m_renderResourceLoader.loadMeshAsync(std::move(mesh)).wait(),
      .material = material.wait(),
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  auto render = std::make_unique<DModel>();
  render->model = m_modelLoader.loadModelAsync(std::move(model)).wait();

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

} // namespace

FactoryPtr createFactory(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader)
{
  return std::make_unique<FactoryImpl>(ecs, modelLoader, renderResourceLoader);
}

} // namespace lithic3d
