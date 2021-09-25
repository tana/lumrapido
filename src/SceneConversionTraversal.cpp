#include "SceneConversionTraversal.h"

#include <cassert>
#include <cstdint>
#include <algorithm>
#include "RayTracingMaterialGroup.h"
#include "utils.h"

SceneConversionTraversal::SceneConversionTraversal(vsg::Device* device)
  : device(device), tlas(vsg::TopLevelAccelerationStructure::create(device)),
    numIndices(0), numVertices(0),
    materialStack({ RayTracingMaterial { vsg::vec3(1.0, 1.0, 1.0) } })
{
}

void SceneConversionTraversal::apply(vsg::Object& object)
{
  object.traverse(*this);
}

void SceneConversionTraversal::apply(vsg::MatrixTransform& transform)
{
  matrixStack.pushAndPostMult(transform.matrix);
  transform.traverse(*this);
  matrixStack.pop();
}

void SceneConversionTraversal::apply(vsg::Geometry& geometry)
{
  addMesh(matrixStack.top(), geometry.arrays, geometry.indices);
}

void SceneConversionTraversal::apply(vsg::VertexIndexDraw& vertexIndexDraw)
{
  addMesh(matrixStack.top(), vertexIndexDraw.arrays, vertexIndexDraw.indices);
}

void SceneConversionTraversal::apply(RayTracingMaterialGroup& rtMatGroup)
{
  materialStack.push(rtMatGroup.material);

  rtMatGroup.traverse(*this);

  materialStack.pop();
}

vsg::ref_ptr<vsg::Array<ObjectInfo>> SceneConversionTraversal::getObjectInfo() const
{
  auto arr = vsg::Array<ObjectInfo>::create(objectInfoList.size());
  std::copy(objectInfoList.begin(), objectInfoList.end(), arr->begin());
  return arr;
}

vsg::ref_ptr<vsg::ushortArray> SceneConversionTraversal::getIndices() const
{
  return concatArray(indicesList);
}

vsg::ref_ptr<vsg::vec3Array> SceneConversionTraversal::getVertices() const
{
  return concatArray(verticesList);
}

vsg::ref_ptr<vsg::vec3Array> SceneConversionTraversal::getNormals() const
{
  return concatArray(normalsList);
}

vsg::ref_ptr<vsg::vec2Array> SceneConversionTraversal::getTexCoords() const
{
  return concatArray(texCoordsList);
}

void SceneConversionTraversal::addMesh(vsg::mat4 transform, vsg::DataList attributes, vsg::ref_ptr<vsg::Data> indices)
{
  vsg::ref_ptr<vsg::Data> vertices = attributes[0];
  vsg::ref_ptr<vsg::Data> normals = attributes[1];
  vsg::ref_ptr<vsg::Data> texCoords = attributes[2];

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
  info.material = materialStack.top();
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
}

