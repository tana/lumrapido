#version 460
#extension GL_EXT_ray_tracing : enable

// Miss shader
// Return background color when ray does not hit any object

// State for Xorshift random number generator
struct RandomState
{
  uint x, y, z, w;
};

layout(location = 0) rayPayloadInEXT RayPayload {
  vec3 color;
  bool traceNextRay;
  vec3 nextOrigin;
  vec3 nextDirection;
  RandomState randomState;
} payload;

void main()
{
  // Generate background color
  vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
  payload.color *= mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), 0.5 * (unitDir.y + 1.0));

  payload.traceNextRay = false;
}