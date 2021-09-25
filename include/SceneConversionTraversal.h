#pragma once

#include <cstdint>
#include <stack>
#include <vsg/core/Value.h>
#include <vsg/core/Inherit.h>
#include <vsg/core/Visitor.h>
#include <vsg/core/Object.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/vk/Device.h>
#include <vsg/vk/State.h>
#include <vsg/Nodes/Geometry.h>
#include <vsg/Nodes/VertexIndexDraw.h>
#include <vsg/Nodes/MatrixTransform.h>
#include <vsg/raytracing/TopLevelAccelerationStructure.h>
#include "RayTracingVisitor.h"
#include "RayTracingMaterialGroup.h"
#include "RayTracingMaterial.h"

struct ObjectInfo
{
  // Offset (first index) of index and vertex attributes of a particular object in respective array
  uint32_t indexOffset;
  uint32_t vertexOffset;
  RayTracingMaterial material;
};

class ObjectInfoValue : public vsg::Inherit<vsg::Value<ObjectInfo>, ObjectInfoValue>
{
};

// A visitor class for converting VSG scene graph into ray tracing data
// Modeled after vsg::BuildAccelerationStructureTraversal class,
// but emits vertex attributes in addition to TLAS.
class SceneConversionTraversal : public RayTracingVisitor
{
public:
  SceneConversionTraversal(vsg::Device* device);

  void apply(vsg::Object& object);
  void apply(vsg::MatrixTransform& transform);
  void apply(vsg::Geometry& geometry);
  void apply(vsg::VertexIndexDraw& vertexIndexDraw);
  void apply(RayTracingMaterialGroup& rtMatGroup);
  
  vsg::ref_ptr<vsg::Array<ObjectInfo>> getObjectInfo() const;
  vsg::ref_ptr<vsg::ushortArray> getIndices() const;
  vsg::ref_ptr<vsg::vec3Array> getVertices() const;
  vsg::ref_ptr<vsg::vec3Array> getNormals() const;
  vsg::ref_ptr<vsg::vec2Array> getTexCoords() const;

  vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;

protected:
  void addMesh(vsg::mat4 transform, vsg::DataList attributes, vsg::ref_ptr<vsg::Data> indices);

  vsg::Device* device;
  
  vsg::MatrixStack matrixStack;

  std::vector<ObjectInfo> objectInfoList;
  std::vector<vsg::ref_ptr<vsg::ushortArray>> indicesList;
  std::vector<vsg::ref_ptr<vsg::vec3Array>> verticesList;
  std::vector<vsg::ref_ptr<vsg::vec3Array>> normalsList;
  std::vector<vsg::ref_ptr<vsg::vec2Array>> texCoordsList;

  uint32_t numIndices;
  uint32_t numVertices;

  std::stack<RayTracingMaterial> materialStack;
};