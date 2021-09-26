#pragma once

#include <vsg/maths/vec3.h>

enum RayTracingMaterialType
{
  RT_MATERIAL_LAMBERT = 0,
  RT_MATERIAL_METAL = 1
};

struct RayTracingMaterial
{
  RayTracingMaterialType type;
  vsg::vec3 color;
};