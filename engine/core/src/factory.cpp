#include "lithic3d/factory.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include <iostream> // TODO

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
    FactoryImpl(Ecs& ecs, RenderResourceLoader& renderResourceLoader);

    MaterialHandle createMaterial(const std::filesystem::path& texturePath) override;
    EntityId createCuboid(const Vec3f& size, MaterialHandle material,
      const Vec2f& textureSize) override;

  private:
    Ecs& m_ecs;
    RenderResourceLoader& m_renderResourceLoader;
};

FactoryImpl::FactoryImpl(Ecs& ecs, RenderResourceLoader& renderResourceLoader)
  : m_ecs(ecs)
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
  material->texture = m_renderResourceLoader.loadTextureAsync(texturePath);

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

  MeshFeatureSet meshFeatures{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };

  auto mesh = render::cuboid(size, textureSize);
  mesh->featureSet = meshFeatures;

  sysRender3d.renderer().compileShader(false, meshFeatures, material.features);

  auto model = std::make_unique<DModel>();

  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .mesh = m_renderResourceLoader.loadMeshAsync(std::move(mesh)).wait(),
      .material = material.resource.wait(),
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  sysRender3d.addEntity(id, std::move(model));

  return id;
}

} // namespace

FactoryPtr createFactory(Ecs& ecs, RenderResourceLoader& renderResourceLoader)
{
  return std::make_unique<FactoryImpl>(ecs, renderResourceLoader);
}

} // namespace lithic3d
