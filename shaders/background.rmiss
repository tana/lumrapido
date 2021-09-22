#version 460
#extension GL_EXT_ray_tracing : enable

// Miss shader
// Return background color when ray does not hit any object

layout(location = 0) rayPayloadInEXT vec3 payload;

void main()
{
  // Generate background color
  vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
  payload = mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), 0.5 * (unitDir.y + 1.0));
}