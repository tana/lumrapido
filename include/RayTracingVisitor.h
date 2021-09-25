#pragma once

#include <vsg/core/Visitor.h>
#include "RayTracingMaterialGroup.h"

class RayTracingVisitor : public vsg::Visitor
{
public:
  virtual void apply(RayTracingMaterialGroup& rtMatGroup) = 0;
};