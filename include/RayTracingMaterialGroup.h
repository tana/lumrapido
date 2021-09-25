#pragma once

#include <vsg/core/Inherit.h>
#include <vsg/nodes/Group.h>
#include <vsg/core/Visitor.h>
#include "RayTracingMaterial.h"

class RayTracingMaterialGroup : public vsg::Inherit<vsg::Group, RayTracingMaterialGroup>
{
public:
  RayTracingMaterialGroup(RayTracingMaterial material);

  virtual void accept(vsg::Visitor& visitor);

  RayTracingMaterial material;
};