#pragma once

#include <vsg/maths/vec3.h>

enum RayTracingMaterialType
{
  RT_MATERIAL_LAMBERT = 0,
  RT_MATERIAL_METAL = 1,
  RT_MATERIAL_DIELECTRIC = 2,
  RT_MATERIAL_PBR = 3
};

struct RayTracingMaterial
{
  RayTracingMaterialType type = RT_MATERIAL_LAMBERT;
  vsg::vec3 color = vsg::vec3(1.0f, 1.0f, 1.0f);
  float fuzz = 0.0f;
  float ior = 1.0f;  // Index of Refraction
  float metallic = 0.0f;
  float roughness = 0.0f;
  int32_t colorTextureIdx = -1;
};