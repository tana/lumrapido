#pragma once

#include <vsg/maths/vec3.h>

enum class AlphaMode {
  Opaque = 0,
  Mask = 1
};

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
  AlphaMode alphaMode = AlphaMode::Opaque;
  float alphaFactor = 1.0f; // Scaling factor for alpha (included in base color factor in glTF)
  float alphaCutoff = 0.5f;
};