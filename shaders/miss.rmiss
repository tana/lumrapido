#version 460
#extension GL_EXT_ray_tracing : enable

#include "common.glsl"

// Miss shader
// Return background color or read environment map when ray does not hit any object

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(binding = 2) uniform Uniforms {
  RayTracingUniform uniforms;
};

layout(binding = 10) uniform sampler2D textures[MAX_NUM_TEXTURES];

void main()
{
  vec3 unitDir = normalize(gl_WorldRayDirectionEXT);

  if (uniforms.envMapTextureIdx >= 0) { // Has environment map
    // Read equirectangular environment map
    // Direction is same as Mitsuba 2 renderer https://mitsuba2.readthedocs.io/en/latest/generated/plugins.html#environment-emitter-envmap
    float phi = 0.5 * (1.0 + safeAtan(unitDir.x, unitDir.z) / PI);
    float theta = acos(unitDir.y) / PI;
    payload.color += payload.multiplier * texture(textures[uniforms.envMapTextureIdx], vec2(phi, theta)).rgb;
  } else {  // No environment map
    // Generate background color
    payload.color += payload.multiplier * mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), 0.5 * (unitDir.y + 1.0));
  }

  payload.traceNextRay = false;
}