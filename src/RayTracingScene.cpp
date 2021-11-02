#include <cassert>
#include "RayTracingScene.h"
#include "utils.h"

RayTracingScene::RayTracingScene(vsg::Device* device)
  : device(device), numIndices(0), numVertices(0)
{
  tlas = vsg::TopLevelAccelerationStructure::create(device);
}

uint32_t RayTracingScene::addMesh(const vsg::mat4& transform, vsg::ref_ptr<vsg::ushortArray> indices, vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals, vsg::ref_ptr<vsg::vec2Array> texCoords, vsg::ref_ptr<vsg::vec4Array> tangents, const RayTracingMaterial& material)
{
  // ID (index of a object)
  uint32_t id = uint32_t(tlas->geometryInstances.size());

  // Vertex positions and indices needed for acceleration structure
  auto accelGeom = vsg::AccelerationGeometry::create();
  accelGeom->verts = vertices;
  accelGeom->indices = indices;
  
  // Create a Bottom-Level Acceleration Structure which represents a mesh object
  auto blas = vsg::BottomLevelAccelerationStructure::create(device);
  blas->geometries.push_back(accelGeom);
  
  // Create an instance of BLAS
  auto instance = vsg::GeometryInstance::create();
  instance->transform = transform;
  instance->accelerationStructure = blas;
  instance->id = id;
  
  // Add the instance into the TLAS
  tlas->geometryInstances.push_back(instance);

  // Store offset information
  ObjectInfo info;
  info.indexOffset = numIndices;
  info.vertexOffset = numVertices;
  info.material = material;
  objectInfoList.push_back(info);

  // Store indices and vertex attributes for closest-hit shader
  indicesList.push_back(indices);
  verticesList.push_back(vertices);
  normalsList.push_back(normals);
  texCoordsList.push_back(texCoords);
  tangentsList.push_back(tangents);

  // Count indices and vertex attributes for offsets
  numIndices += uint32_t(indices->valueCount());
  numVertices += uint32_t(vertices->valueCount());

  assert(tlas->geometryInstances.size() == objectInfoList.size());
  assert(tlas->geometryInstances.size() == indicesList.size());
  assert(tlas->geometryInstances.size() == verticesList.size());
  assert(tlas->geometryInstances.size() == normalsList.size());
  assert(tlas->geometryInstances.size() == texCoordsList.size());
  assert(tlas->geometryInstances.size() == tangentsList.size());

  return id;
}

uint32_t RayTracingScene::addMesh(const vsg::mat4& transform, vsg::ref_ptr<vsg::ushortArray> indices, vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals, vsg::ref_ptr<vsg::vec2Array> texCoords, const RayTracingMaterial& material)
{
  auto tangents = vsg::vec4Array::create(vertices->valueCount()); // Create tangent data with default value of vec4
  return addMesh(transform, indices, vertices, normals, texCoords, tangents, material);
}

uint32_t RayTracingScene::addTexture(const vsg::ImageInfo& imageInfo)
{
  textures.push_back(imageInfo);

  return uint32_t(textures.size() - 1);
}

uint32_t RayTracingScene::addTexture(vsg::ref_ptr<vsg::Data> imageData, vsg::ref_ptr<vsg::Sampler> sampler)
{
  auto image = vsg::Image::create(imageData);
  image->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image->tiling = VK_IMAGE_TILING_LINEAR;

  auto imageView = vsg::ImageView::create(image);

  return addTexture(vsg::ImageInfo(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

vsg::ref_ptr<vsg::Array<ObjectInfo>> RayTracingScene::getObjectInfo() const
{
  auto arr = vsg::Array<ObjectInfo>::create(uint32_t(objectInfoList.size()));
  std::copy(objectInfoList.begin(), objectInfoList.end(), arr->begin());
  return arr;
}

vsg::ref_ptr<vsg::ushortArray> RayTracingScene::getIndices() const
{
  return concatArray(indicesList);
}

vsg::ref_ptr<vsg::vec3Array> RayTracingScene::getVertices() const
{
  return concatArray(verticesList);
}

vsg::ref_ptr<vsg::vec3Array> RayTracingScene::getNormals() const
{
  return concatArray(normalsList);
}

vsg::ref_ptr<vsg::vec2Array> RayTracingScene::getTexCoords() const
{
  return concatArray(texCoordsList);
}

vsg::ref_ptr<vsg::vec4Array> RayTracingScene::getTangents() const
{
  return concatArray(tangentsList);
}

vsg::ref_ptr<vsg::vec4Array2D> RayTracingScene::getEnvMap() const
{
  return envMap;
}

void RayTracingScene::setEnvMap(vsg::ref_ptr<vsg::vec4Array2D> envMap)
{
  this->envMap = envMap;

  envMapSamplingData = EnvMapSamplingData::create(envMap);
}
