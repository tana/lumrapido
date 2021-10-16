#pragma once

#include <vsg/maths/vec3.h>

struct RayTracingMaterial
{
  vsg::vec3 color = vsg::vec3(1.0f, 1.0f, 1.0f);  // Base color
  float metallic = 0.0f;
  float roughness = 0.0f;
  int32_t colorTextureIdx = -1;
  int32_t metallicRoughnessTextureIdx = -1;
  int32_t normalTextureIdx = -1;
  float normalTextureScale = 1.0f;
  int32_t emissiveTextureIdx = -1;
  vsg::vec3 emissive = vsg::vec3(0.0f, 0.0f, 0.0f);
};