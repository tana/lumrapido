#version 460
#extension GL_EXT_ray_tracing : enable
// For uint16_t type
#extension GL_EXT_shader_16bit_storage : enable
// For layout qualifier "scalar", which aligns vec3 as vec3, not as vec4
#extension GL_EXT_scalar_block_layout : enable

// Closest hit shader

const int RT_MATERIAL_LAMBERT = 0;
const int RT_MATERIAL_METAL = 1;

struct Material
{
  int type;
  vec3 color;
};

struct ObjectInfo
{
  uint indexOffset;
  uint vertexOffset;
  Material material;
};

// State for Xorshift random number generator
struct RandomState
{
  uint x, y, z, w;
};

layout(binding = 3, scalar) buffer ObjectInfos {
  ObjectInfo objectInfos[];
};
layout(binding = 4, scalar) buffer Indices {
  uint16_t indices[];
};
layout(binding = 5, scalar) buffer Vertices {
  vec3 vertices[];
};
layout(binding = 6, scalar) buffer Normals {
  vec3 normals[];
};
layout(binding = 7, scalar) buffer TexCoords {
  vec2 texCoords[];
};

layout(location = 0) rayPayloadInEXT RayPayload {
  vec3 color;
  bool traceNextRay;
  vec3 nextOrigin;
  vec3 nextDirection;
  RandomState randomState;
} payload;

hitAttributeEXT vec2 uv;  // Barycentric coordinate of the hit position inside a triangle

// Pseudo-random number using Xorshift (xor128)
// G. Marsaglia, "Xorshift RNGs", Journal of Statistical Software, vol. 8, no. 14, pp. 1-6, 2003, doi: 10.18637/jss.v008.i14
uint random(inout RandomState state)
{
  uint t = (state.x ^ (state.x << 11));
  state.x = state.y;
  state.y = state.z;
  state.z = state.w;
  return (state.w = (state.w ^ (state.w >> 19)) ^ (t ^ (t >> 8)));
}

float randomFloat(inout RandomState state, float minimum, float maximum)
{
  return minimum + (float(random(state)) / 4294967296.0) * (maximum - minimum);
}

// Sample a random point in a sphere with radius of 1
vec3 randomPointInUnitSphere(inout RandomState state)
{
  // Rejection sampling
  while (true) {
    // Random point in a cube
    vec3 point = vec3(randomFloat(state, -1.0, 1.0), randomFloat(state, -1.0, 1.0), randomFloat(state, -1.0, 1.0));
    // Accept if the point is in a unit sphere
    if (dot(point, point) < 1.0) return point;
  }
}

void main()
{
  uint indexOffset = objectInfos[gl_InstanceID].indexOffset;
  uint vertexOffset = objectInfos[gl_InstanceID].vertexOffset;

  uint idx0 = uint(indices[indexOffset + 3 * gl_PrimitiveID]);
  uint idx1 = uint(indices[indexOffset + 3 * gl_PrimitiveID + 1]);
  uint idx2 = uint(indices[indexOffset + 3 * gl_PrimitiveID + 2]);

  vec3 normal0 = normals[vertexOffset + idx0];
  vec3 normal1 = normals[vertexOffset + idx1];
  vec3 normal2 = normals[vertexOffset + idx2];

  Material material = objectInfos[gl_InstanceID].material;
  
  // Normal vector in object coordinate (interpolated from barycentric coords)
  vec3 normalObj = (1.0 - uv.x - uv.y) * normal0 + uv.x * normal1 + uv.y * normal2;
  // Normal vector in world coordinate
  vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(normalObj, 0.0));

  // Point of intersection
  vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

  if (material.type == RT_MATERIAL_LAMBERT) {
    // Randomly generate a new ray
    payload.nextDirection = normal + normalize(randomPointInUnitSphere(payload.randomState));
    payload.nextOrigin = hitPoint;
    // Signal the caller (ray generation shader) to trace next ray, instead of recursively tracing
    payload.color *= material.color;
    payload.traceNextRay = true;
  } else if (material.type == RT_MATERIAL_METAL) {
    payload.nextDirection = reflect(normalize(gl_WorldRayDirectionEXT), normal);
    payload.nextOrigin = hitPoint;
    payload.color *= material.color;
    payload.traceNextRay = true;
  } else {  // Unknown material
    payload.color *= vec3(0.0);
    payload.traceNextRay = false;
  }
}