#include "lithic3d/factory.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/ecs.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/renderer.hpp"

namespace lithic3d
{

using render::Material;
using render::MaterialPtr;
using render::MaterialHandle;
using render::bitflag;
using render::BufferUsage;

namespace
{

class FactoryImpl : public Factory
{
  public:
    FactoryImpl(Ecs& ecs, const FileSystem& fileSystem);

    MaterialHandle constructMaterial(const std::filesystem::path& texturePath) override;
    MaterialHandle constructMaterial(const std::filesystem::path& texturePath,
      const std::filesystem::path& normalMapPath) override;

    void constructCuboid(EntityId id, const MaterialHandle& material, const Vec3f& size) override;

  private:
    Ecs& m_ecs;
    const FileSystem& m_fileSystem;
};

FactoryImpl::FactoryImpl(Ecs& ecs, const FileSystem& fileSystem)
  : m_ecs(ecs)
  , m_fileSystem(fileSystem) {}

MaterialHandle FactoryImpl::constructMaterial(const std::filesystem::path& texturePath)
{
  auto& renderer = m_ecs.system<SysRender3d>().renderer();

  render::MaterialFeatureSet materialFeatures{
    .flags{
      bitflag(render::MaterialFeatures::HasTexture)
    }
  };
  auto texture = render::loadTexture(m_fileSystem.readAppDataFile(texturePath));
  auto material = std::make_unique<Material>(materialFeatures);
  material->texture.id = renderer.addTexture(std::move(texture));

  return renderer.addMaterial(std::move(material));
}

MaterialHandle FactoryImpl::constructMaterial(const std::filesystem::path& texturePath,
  const std::filesystem::path& normalMapPath)
{
  auto& renderer = m_ecs.system<SysRender3d>().renderer();

  render::MaterialFeatureSet materialFeatures{
    .flags{
      bitflag(render::MaterialFeatures::HasTexture) |
      bitflag(render::MaterialFeatures::HasNormalMap)
    }
  };

  auto texture = render::loadTexture(m_fileSystem.readAppDataFile(texturePath));
  auto normalMap = render::loadTexture(m_fileSystem.readAppDataFile(normalMapPath));

  auto material = std::make_unique<Material>(materialFeatures);
  material->texture.id = renderer.addTexture(std::move(texture));
  material->normalMap.id = renderer.addNormalMap(std::move(normalMap));

  return renderer.addMaterial(std::move(material));
}

void FactoryImpl::constructCuboid(EntityId id, const MaterialHandle& material, const Vec3f& size)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender3d = m_ecs.system<SysRender3d>();

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();

  sysSpatial.addEntity(id, spatial);

  auto mesh = render::cuboid(size[0], size[1], size[2], { 1.f, 1.f });

  mesh->featureSet = render::MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };

  sysRender3d.renderer().compileShader(false, mesh->featureSet, material.features);

  auto model = std::make_unique<DModel>();

  model->submodels.push_back(
    Submodel{
      .mesh = sysRender3d.renderer().addMesh(std::move(mesh)),
      .material = material,
      .skin = nullptr,
      .jointTransforms{}
    }
  );

  sysRender3d.addEntity(id, std::move(model));
}

} // namespace

FactoryPtr createFactory(Ecs& ecs, FileSystem& fileSystem)
{
  return std::make_unique<FactoryImpl>(ecs, fileSystem);
}

} // namespace lithic3d
