#include <lithic3d/renderables.hpp>
#include <gtest/gtest.h>

using namespace lithic3d;
using namespace lithic3d::render;

class MeshTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(MeshTest, calcOffsetInVertex_first_attribute_returns_zero)
{
  VertexLayout layout{
    BufferUsage::AttrPosition,
    BufferUsage::AttrNormal,
    BufferUsage::AttrTexCoord
  };

  auto offset = calcOffsetInVertex(layout, BufferUsage::AttrPosition);
  EXPECT_EQ(0, offset);
}

TEST_F(MeshTest, calcOffsetInVertex_second_attribute)
{
  VertexLayout layout{
    BufferUsage::AttrPosition,
    BufferUsage::AttrNormal,
    BufferUsage::AttrTexCoord
  };

  auto offset = calcOffsetInVertex(layout, BufferUsage::AttrNormal);
  EXPECT_EQ(sizeof(Vec3f), offset);
}

TEST_F(MeshTest, calcOffsetInVertex_third_attribute)
{
  VertexLayout layout{
    BufferUsage::AttrPosition,
    BufferUsage::AttrNormal,
    BufferUsage::AttrTexCoord
  };

  auto offset = calcOffsetInVertex(layout, BufferUsage::AttrTexCoord);
  EXPECT_EQ(sizeof(Vec3f) + sizeof(Vec3f), offset);
}

TEST_F(MeshTest, calcOffsetInVertex_with_missing_attribute)
{
  VertexLayout layout{
    BufferUsage::AttrPosition,
    BufferUsage::AttrTexCoord
  };

  auto offset = calcOffsetInVertex(layout, BufferUsage::AttrTexCoord);
  EXPECT_EQ(sizeof(Vec3f), offset);
}

TEST_F(MeshTest, createVertexArray_single_vertex_all_attributes)
{
  Mesh mesh;
  mesh.featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags = 0
  };
  mesh.attributeBuffers.emplace_back(Buffer{AlignedBytes{std::vector<Vec3f>{Vec3f{ 1, 2, 3 }}},
    BufferUsage::AttrPosition});
  mesh.attributeBuffers.emplace_back(Buffer{AlignedBytes{std::vector<Vec3f>{Vec3f{ 4, 5, 6 }}},
    BufferUsage::AttrNormal});
  mesh.attributeBuffers.emplace_back(Buffer{AlignedBytes{std::vector<Vec2f>{Vec2f{ 7, 8 }}},
    BufferUsage::AttrTexCoord});
  auto vertexData = createVertexArray(mesh);

  struct Vertex
  {
    Vec3f pos;
    Vec3f normal;
    Vec2f texCoord;
  };

  Vertex vertex;
  char* ptr = vertexData.data();
  std::memcpy(&vertex.pos, ptr, sizeof(Vec3f));
  ptr += sizeof(Vec3f);
  std::memcpy(&vertex.normal, ptr, sizeof(Vec3f));
  ptr += sizeof(Vec3f);
  std::memcpy(&vertex.texCoord, ptr, sizeof(Vec2f));

  EXPECT_EQ(Vec3f({ 1, 2, 3 }), vertex.pos);
  EXPECT_EQ(Vec3f({ 4, 5, 6 }), vertex.normal);
  EXPECT_EQ(Vec2f({ 7, 8 }), vertex.texCoord);
}

TEST_F(MeshTest, createVertexArray_two_vertices)
{
  Mesh mesh;
  mesh.featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags = 0
  };
  mesh.attributeBuffers.emplace_back(Buffer{
    AlignedBytes{std::vector<Vec3f>{
      Vec3f{ 1, 2, 3 },
      Vec3f{ 4, 5, 6 }
    }},
    BufferUsage::AttrPosition
  });
  mesh.attributeBuffers.emplace_back(Buffer{
    AlignedBytes{std::vector<Vec3f>{
      Vec3f{ 7, 8, 9 },
      Vec3f{ 10, 11, 12 }
    }},
    BufferUsage::AttrNormal
  });
  mesh.attributeBuffers.emplace_back(Buffer{
    AlignedBytes{std::vector<Vec2f>{
      Vec2f{ 13, 14 },
      Vec2f{ 15, 16 }
    }},
    BufferUsage::AttrTexCoord
  });
  auto vertexData = createVertexArray(mesh);

  struct Vertex
  {
    Vec3f pos;
    Vec3f normal;
    Vec2f texCoord;
  };

  std::array<Vertex, 2> vertices;
  char* ptr = vertexData.data();
  std::memcpy(&vertices[0].pos, ptr, sizeof(Vec3f));
  ptr += sizeof(Vec3f);
  std::memcpy(&vertices[0].normal, ptr, sizeof(Vec3f));
  ptr += sizeof(Vec3f);
  std::memcpy(&vertices[0].texCoord, ptr, sizeof(Vec2f));
  ptr += sizeof(Vec2f);
  std::memcpy(&vertices[1].pos, ptr, sizeof(Vec3f));
  ptr += sizeof(Vec3f);
  std::memcpy(&vertices[1].normal, ptr, sizeof(Vec3f));
  ptr += sizeof(Vec3f);
  std::memcpy(&vertices[1].texCoord, ptr, sizeof(Vec2f));

  EXPECT_EQ(Vec3f({ 1, 2, 3 }), vertices[0].pos);
  EXPECT_EQ(Vec3f({ 7, 8, 9 }), vertices[0].normal);
  EXPECT_EQ(Vec2f({ 13, 14 }), vertices[0].texCoord);
  EXPECT_EQ(Vec3f({ 4, 5, 6 }), vertices[1].pos);
  EXPECT_EQ(Vec3f({ 10, 11, 12 }), vertices[1].normal);
  EXPECT_EQ(Vec2f({ 15, 16 }), vertices[1].texCoord);
}
