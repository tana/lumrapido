#version 460
#extension GL_EXT_ray_tracing : enable

#include "common.glsl"

// Miss shader
// Return background color or read environment map when ray does not hit any object

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(binding = BINDING_ENV_MAP) uniform sampler2D envMap;

void main()
{
  vec3 unitDir = normalize(gl_WorldRayDirectionEXT);

  // Read equirectangular environment map
  // Direction is same as Mitsuba 2 renderer https://mitsuba2.readthedocs.io/en/latest/generated/plugins.html#environment-emitter-envmap
  float phi = 0.5 * (1.0 + safeAtan(unitDir.x, unitDir.z) / PI);
  float theta = acos(unitDir.y) / PI;
  payload.color += payload.multiplier * texture(envMap, vec2(phi, theta)).rgb;

  payload.traceNextRay = false;
}