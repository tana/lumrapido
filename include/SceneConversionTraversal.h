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
#include "RayTracingVisitor.h"
#include "RayTracingMaterialGroup.h"
#include "RayTracingMaterial.h"
#include "RayTracingScene.h"

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
  
  vsg::ref_ptr<RayTracingScene> scene;
protected:
  void addMesh(vsg::mat4 transform, vsg::DataList attributes, vsg::ref_ptr<vsg::Data> indices);

  vsg::Device* device;
  
  vsg::MatrixStack matrixStack;

  std::stack<RayTracingMaterial> materialStack;
};