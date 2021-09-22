#version 460
#extension GL_EXT_ray_tracing : enable

// Closest hit shader

layout(location = 0) rayPayloadInEXT vec3 payload;

hitAttributeEXT vec2 barycentricCoord;  // Barycentric coordinate of the hit position inside a triangle

void main()
{
  payload = vec3(1.0, 1.0, 1.0);
}