#version 460
#extension GL_EXT_ray_tracing : enable
// For uint16_t type
#extension GL_EXT_shader_16bit_storage : enable
// For layout qualifier "scalar", which aligns vec3 as vec3, not as vec4
#extension GL_EXT_scalar_block_layout : enable

#include "common.glsl"

// Closest hit shader

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

layout(location = 0) rayPayloadInEXT RayPayload payload;

hitAttributeEXT vec2 uv;  // Barycentric coordinate of the hit position inside a triangle

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

// cosTheta = dot(vectorToEye, normal), eta = n1/n2
float fresnel(in float cosTheta, in float eta)
{
  // sqrt(f0) = (n1-n2)/(n1+n2) = (eta-1)/(eta+1)
  float sqrtF0 = (eta - 1) / (eta + 1);
  float f0 = sqrtF0 * sqrtF0;
  // Schlick's approximation
  return f0 + (1 - f0) * pow(1 - cosTheta, 5);
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

  bool isFront = gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT;
  
  // Normal vector in object coordinate (interpolated from barycentric coords)
  vec3 normalObj = (1.0 - uv.x - uv.y) * normal0 + uv.x * normal1 + uv.y * normal2;
  // Normal vector in world coordinate
  vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(normalObj, 0.0));
  // Reverse normal vector if the surface is facing back
  if (!isFront) {
    normal = -normal;
  }

  // Point of intersection
  vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
  // Normalized direction of incoming ray
  vec3 unitRayDir = normalize(gl_WorldRayDirectionEXT);

  if (material.type == RT_MATERIAL_LAMBERT) {
    // Randomly generate a new ray
    payload.nextDirection = normal + normalize(randomPointInUnitSphere(payload.randomState));
    if (nearZero(payload.nextDirection)) {
      payload.nextDirection = normal;
    }
    payload.nextOrigin = hitPoint;
    // Signal the caller (ray generation shader) to trace next ray, instead of recursively tracing
    payload.color *= material.color;
    payload.traceNextRay = true;
  } else if (material.type == RT_MATERIAL_METAL) {
    vec3 reflectedDir = reflect(unitRayDir, normal) + material.fuzz * randomPointInUnitSphere(payload.randomState);
    if (dot(reflectedDir, normal) > 0) {
      payload.nextDirection = reflectedDir;
      payload.nextOrigin = hitPoint;
      payload.color *= material.color;
      payload.traceNextRay = true;
    } else {  // When the reflected ray goes inside the object
      payload.color = vec3(0.0);
      payload.traceNextRay = false;
    }
  } else if (material.type == RT_MATERIAL_DIELECTRIC) {
    float eta = isFront ? (1 / material.ior) : material.ior;
    vec3 refractedDir = refract(unitRayDir, normal, eta);
    if (nearZero(refractedDir) || fresnel(dot(-unitRayDir, normal), eta) > randomFloat(payload.randomState, 0.0, 1.0)) {
      // Total internal reflection or fresnel
      refractedDir = reflect(unitRayDir, normal);
    }
    payload.nextDirection = refractedDir;
    payload.nextOrigin = hitPoint;
    payload.traceNextRay = true;
  } else {  // Unknown material
    payload.color *= vec3(0.0);
    payload.traceNextRay = false;
  }
}