#include <cassert>
#include "RayTracingScene.h"
#include "utils.h"

RayTracingScene::RayTracingScene(vsg::Device* device)
  : device(device), numIndices(0), numVertices(0)
{
  tlas = vsg::TopLevelAccelerationStructure::create(device);
}

uint32_t RayTracingScene::addMesh(const vsg::mat4& transform, vsg::ref_ptr<vsg::Data> indices, vsg::ref_ptr<vsg::Data> vertices, vsg::ref_ptr<vsg::Data> normals, vsg::ref_ptr<vsg::Data> texCoords, const RayTracingMaterial& material)
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
  indicesList.push_back(indices.cast<vsg::ushortArray>());
  verticesList.push_back(vertices.cast<vsg::vec3Array>());
  normalsList.push_back(normals.cast<vsg::vec3Array>());
  texCoordsList.push_back(texCoords.cast<vsg::vec2Array>());

  // Count indices and vertex attributes for offsets
  numIndices += uint32_t(indices->valueCount());
  numVertices += uint32_t(vertices->valueCount());

  assert(tlas->geometryInstances.size() == objectInfoList.size());
  assert(tlas->geometryInstances.size() == indicesList.size());
  assert(tlas->geometryInstances.size() == verticesList.size());
  assert(tlas->geometryInstances.size() == normalsList.size());
  assert(tlas->geometryInstances.size() == texCoordsList.size());

  return id;
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
