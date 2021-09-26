#version 460
#extension GL_EXT_ray_tracing : enable

#include "common.glsl"

// Miss shader
// Return background color when ray does not hit any object

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
  // Generate background color
  vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
  payload.color *= mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), 0.5 * (unitDir.y + 1.0));

  payload.traceNextRay = false;
}