#pragma once

#include <cstdint>
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

struct ObjectInfo
{
  // Offset (first index) of index and vertex attributes of a particular object in respective array
  uint32_t indexOffset;
  uint32_t vertexOffset;
};

class ObjectInfoValue : public vsg::Inherit<vsg::Value<ObjectInfo>, ObjectInfoValue>
{
};

// A visitor class for converting VSG scene graph into ray tracing data
// Modeled after vsg::BuildAccelerationStructureTraversal class,
// but emits vertex attributes in addition to TLAS.
class SceneConversionTraversal : public vsg::Visitor
{
public:
  SceneConversionTraversal(vsg::Device* device);

  void apply(vsg::Object& object);
  void apply(vsg::MatrixTransform& transform);
  void apply(vsg::Geometry& geometry);
  void apply(vsg::VertexIndexDraw& vertexIndexDraw);

  vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;
  vsg::DataList objectInfoList;
  vsg::DataList indicesList;
  vsg::DataList verticesList;
  vsg::DataList normalsList;
  vsg::DataList texCoordsList;

protected:
  void addMesh(vsg::mat4 transform, vsg::DataList attributes, vsg::ref_ptr<vsg::Data> indices);

  vsg::Device* device;
  
  vsg::MatrixStack matrixStack;

  uint32_t numIndices;
  uint32_t numVertices;
};