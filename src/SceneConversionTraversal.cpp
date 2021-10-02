#include "SceneConversionTraversal.h"

#include <cassert>
#include <cstdint>
#include <algorithm>
#include "RayTracingMaterialGroup.h"
#include "utils.h"

SceneConversionTraversal::SceneConversionTraversal(vsg::Device* device)
  : device(device), materialStack({ RayTracingMaterial { RT_MATERIAL_LAMBERT, vsg::vec3(1.0, 1.0, 1.0) } })
{
  scene = RayTracingScene::create(device);
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
  scene->addMesh(matrixStack.top(), geometry.indices, geometry.arrays[0], geometry.arrays[1], geometry.arrays[2], materialStack.top());
}

void SceneConversionTraversal::apply(vsg::VertexIndexDraw& vertexIndexDraw)
{
  scene->addMesh(matrixStack.top(), vertexIndexDraw.indices, vertexIndexDraw.arrays[0], vertexIndexDraw.arrays[1], vertexIndexDraw.arrays[2], materialStack.top());
}

void SceneConversionTraversal::apply(RayTracingMaterialGroup& rtMatGroup)
{
  materialStack.push(rtMatGroup.material);

  rtMatGroup.traverse(*this);

  materialStack.pop();
}
