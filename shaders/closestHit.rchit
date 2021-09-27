#version 460
#extension GL_EXT_ray_tracing : enable
// For uint16_t type
#extension GL_EXT_shader_16bit_storage : enable
// For layout qualifier "scalar", which aligns vec3 as vec3, not as vec4
#extension GL_EXT_scalar_block_layout : enable

#include "common.glsl"

// Closest hit shader
// Implementation of physically based rendering (RT_MATERIAL_PBR) is based on the following papers:
//  B. Burley, "Physically Based Shading at Disney", in SIGGRAPH 2012 Course: Practical Physically Based Shading in Film and Game production, 2012, https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
//  B. Karis, "Real Shading in Unreal Engine 4", in SIGGRAPH 2013 Course: Physically Based Shading in Theory and Practice, 2013, https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
//  C. Schlick, "An Inexpensive BRDF Model for Physically-based Rendering", in Computer Graphics Forum, vol. 13, no. 3, 1994, pp. 233-246.

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

// Calculate Fresnel term using Schlick's approximation
// cosTheta = dot(vectorToEye, normal)
vec3 fresnelSchlick(in float cosTheta, in vec3 f0)
{
  return f0 + (1 - f0) * pow(1 - cosTheta, 5);
}

// GGX/Trowbridge-Reitz normal distribution function D(h)
float normalDistGGX(in vec3 halfwayVec, in vec3 normal, float roughness)
{
  float alpha = roughness * roughness;
  float alphaSq = alpha * alpha;
  float dotNH = dot(normal, halfwayVec);
  float ggxTemp = dotNH * dotNH * (alphaSq - 1.0) + 1.0;
  return alphaSq / (PI * ggxTemp * ggxTemp);
}

// Schlick GGX Geometric attenuation term G(l,v,h)
float geometricAttenuationSchlick(in vec3 lightVec, in vec3 viewVec, in vec3 normal, float roughness)
{
  // Halfway vector
  vec3 halfwayVec = normalize(viewVec + lightVec);

  float dotNL = dot(normal, lightVec);
  float dotNV = dot(normal, viewVec);
  float alpha = roughness * roughness;
  float k = alpha / 2.0;
  float g1L = dotNL / (dotNL * (1.0 - k) + k);
  float g1V = dotNV / (dotNV * (1.0 - k) + k);
  return g1L * g1V;
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
    // sqrt(f0) = (n1-n2)/(n1+n2) = (eta-1)/(eta+1)
    float sqrtF0 = (eta - 1) / (eta + 1);
    float f0 = sqrtF0 * sqrtF0;
    vec3 refractedDir = refract(unitRayDir, normal, eta);
    float fresnel = fresnelSchlick(dot(-unitRayDir, normal), vec3(f0)).x;
    if (nearZero(refractedDir) || fresnel > randomFloat(payload.randomState, 0.0, 1.0)) {
      // Total internal reflection or Fresnel
      refractedDir = reflect(unitRayDir, normal);
    }
    payload.nextDirection = refractedDir;
    payload.nextOrigin = hitPoint;
    payload.traceNextRay = true;
  } else if (material.type == RT_MATERIAL_PBR) {
    // Vector towards the camera
    vec3 viewVec = -unitRayDir;
    // Vector of incoming light (from point on the point of intersection to light source or another object)
    vec3 lightVec = normal + randomPointInUnitSphere(payload.randomState);
    if (nearZero(lightVec)) lightVec = normal;
    lightVec = normalize(lightVec);
    // Halfway vector
    vec3 halfwayVec = normalize(viewVec + lightVec);
    // Normal distribution function D(h)
    float normalDist = normalDistGGX(normal, halfwayVec, material.roughness);
    // Geometric attenuation term G(l,v,h)
    float geometricAttenuation = geometricAttenuationSchlick(lightVec, viewVec, normal, material.roughness);
    // Fresnel factor F(v,h)
    vec3 f0 = mix(vec3(0.04), material.color, material.metallic);
    vec3 fresnel = fresnelSchlick(dot(viewVec, halfwayVec), f0);
    // Final BRDF calculation
    float dotNL = dot(normal, lightVec);
    float dotNV = dot(normal, viewVec);
    vec3 diffuseBRDF = material.color / PI;
    vec3 specularBRDF = normalDist * fresnel * geometricAttenuation / (4.0 * dotNL * dotNV);
    vec3 brdf = diffuseBRDF * fresnelSchlick(dotNV, f0) + specularBRDF;
    // Trace next ray
    payload.color *= brdf;
    payload.nextOrigin = hitPoint;
    payload.nextDirection = lightVec;
    payload.traceNextRay = true;
  } else {  // Unknown material
    payload.color *= vec3(0.0);
    payload.traceNextRay = false;
  }
}