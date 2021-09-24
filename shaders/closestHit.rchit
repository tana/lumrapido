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

layout(location = 0) rayPayloadInEXT vec3 payload;

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
  
  // Normal vector in object coordinate
  vec3 normalObj = (1.0 - uv.x - uv.y) * normal0 + uv.x * normal1 + uv.y * normal2;
  // Normal vector in world coordinate
  vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(normalObj, 0.0));

  payload = 0.5 * normal + 0.5;
}