#pragma once

#include <vsg/maths/vec3.h>
#include <vsg/core/Inherit.h>
#include <vsg/core/Value.h>

struct RayTracingMaterial
{
  vsg::vec3 color;
};

class RayTracingMaterialValue : public vsg::Inherit<vsg::Value<RayTracingMaterial>, RayTracingMaterialValue>
{
};