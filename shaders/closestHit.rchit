#version 460
#extension GL_EXT_ray_tracing : enable

// Closest hit shader

struct ObjectInfo {
  uint indexOffset;
  uint vertexOffset;
};

// Because GLSL requires vec3 to be aligned to 16-byte (4 x float) boundary,
// MyVec3 is used to densely pack vec3 in buffers.
// See:
//  https://stackoverflow.com/questions/38172696/should-i-ever-use-a-vec3-inside-of-a-uniform-buffer-or-shader-storage-buffer-o
//  https://qiita.com/RT-EGG/items/74b2e68a9bfd4bdfa3ab
struct MyVec3 {
  float x;
  float y;
  float z;
};

// State for Xorshift random number generator
struct RandomState
{
  uint x, y, z, w;
};

layout(binding = 3, std430) buffer ObjectInfos {
  ObjectInfo objectInfos[];
};
layout(binding = 4, std430) buffer Indices {
  uint indices[]; // Actually 16-bit integers
};
layout(binding = 5, std430) buffer Vertices {
  MyVec3 vertices[];
};
layout(binding = 6, std430) buffer Normals {
  MyVec3 normals[];
};
layout(binding = 7, std430) buffer TexCoords {
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

// Convert a MyVec3 into a vec3
vec3 myVec3ToVec3(MyVec3 val)
{
  return vec3(val.x, val.y, val.z);
}

// Read a 16-bit unsigned integer from indices array.
// It treats a 32-bit uint as two 16-bit integers.
// This is because GLSL does not have 16-bit integers.
// (It assumes GPU uses little-endian format)
uint readIndex(uint i)
{
  return (i % 2 == 0) ? (indices[i / 2] & 0x0000FFFF) : (indices[i / 2] >> 16);
}

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

  uint idx0 = readIndex(indexOffset + 3 * gl_PrimitiveID);
  uint idx1 = readIndex(indexOffset + 3 * gl_PrimitiveID + 1);
  uint idx2 = readIndex(indexOffset + 3 * gl_PrimitiveID + 2);

  vec3 normal0 = myVec3ToVec3(normals[vertexOffset + idx0]);
  vec3 normal1 = myVec3ToVec3(normals[vertexOffset + idx1]);
  vec3 normal2 = myVec3ToVec3(normals[vertexOffset + idx2]);
  
  // Normal vector in object coordinate (interpolated from barycentric coords)
  vec3 normalObj = (1.0 - uv.x - uv.y) * normal0 + uv.x * normal1 + uv.y * normal2;
  // Normal vector in world coordinate
  vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(normalObj, 0.0));

  // Point of intersection
  vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

  // Randomly generate a new ray
  payload.nextDirection = normal + normalize(randomPointInUnitSphere(payload.randomState));
  payload.nextOrigin = hitPoint;
  // Signal the caller (ray generation shader) to trace next ray, instead of recursively tracing
  payload.color *= vec3(0.5);  // TODO: reflectance
  payload.traceNextRay = true;
}